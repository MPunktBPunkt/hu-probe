/**
 * @file DiagLog.cpp
 * @brief Implementation of the DiagLog PSRAM/RTC rings and flash snapshots.
 */
#include "core/DiagLog.h"
#include "core/HuMem.h"
#include "core/PanicTail.h"
#include "core/PhaseGate.h"
#include "BuildFlags.h"

#include <esp_attr.h>
#include <esp_partition.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x48554C47;  // HULG
constexpr uint32_t kSnapMagic = 0x48554C53;  // HULS
constexpr uint16_t kSnapVersion = 1;
constexpr size_t kRtcBytes = 2048;
constexpr size_t kRingBytes = 64 * 1024;
constexpr size_t kLineMax = 192;
constexpr uint8_t kSlots = 8;
constexpr size_t kSlotBytes = 4096;

/** @brief On-flash snapshot header; text follows up to kSlotBytes. */
struct __attribute__((packed)) SnapHdr {
    uint32_t magic;
    uint16_t version;
    uint16_t textLen;
    uint32_t seq;
    uint32_t bootGeneration;
    uint32_t uptimeMs;
    char reason[16];
    char resetReason[20];
    char restartCause[20];
    char fw[16];
    uint32_t crc;
};

/** @brief 2 KiB byte ring in RTC no-init memory; survives software reset, panic and watchdog. */
struct RtcRing {
    uint32_t magic;
    uint16_t head;
    uint16_t len;
    char data[kRtcBytes];
};

RTC_NOINIT_ATTR RtcRing rtc;
char prevTail[kRtcBytes + 1] = {0};
char sentForSid[64] = {0};

char* ring = nullptr;
size_t ringHead = 0;  // next write position
size_t ringLen = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

const esp_partition_t* snapPart = nullptr;
char* slotBuf = nullptr;
uint32_t slotSeq[kSlots] = {};
uint32_t nextSeq = 1;
unsigned long lastPersistMs = 0;
volatile bool persistBusy = false;

/**
 * @brief CRC-32 (same polynomial as TripStore) over n bytes.
 * @param data  Source bytes.
 * @param n     Length.
 * @return CRC of the range.
 */
uint32_t crc32(const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/**
 * @brief Write n bytes to the snapshot partition, bouncing through internal RAM.
 * @param off  Offset in the partition.
 * @param src  Source (may be PSRAM).
 * @param n    Byte count.
 * @return true on success.
 */
bool writeFlash(uint32_t off, const void* src, size_t n) {
    uint8_t bounce[256];
    const uint8_t* s = static_cast<const uint8_t*>(src);
    while (n) {
        const size_t k = n < sizeof(bounce) ? n : sizeof(bounce);
        memcpy(bounce, s, k);
        if (esp_partition_write(snapPart, off, bounce, k) != ESP_OK) return false;
        off += k;
        s += k;
        n -= k;
    }
    return true;
}

/**
 * @brief Append src into dst without exceeding cap; used while building a snapshot body.
 * @param[in,out] dst  Body buffer.
 * @param cap          Capacity of dst.
 * @param[in,out] used Bytes already used.
 * @param src          NUL-terminated text to append; nullptr is ignored.
 */
void appendText(char* dst, size_t cap, size_t& used, const char* src) {
    if (!src || used >= cap) return;
    const size_t n = strnlen(src, cap - used);
    memcpy(dst + used, src, n);
    used += n;
}

/**
 * @brief Load seq numbers from the 8 flash slots into slotSeq[] and set nextSeq.
 */
void scanSlots() {
    memset(slotSeq, 0, sizeof(slotSeq));
    nextSeq = 1;
    if (!snapPart) return;
    uint32_t maxSeq = 0;
    for (uint8_t i = 0; i < kSlots; i++) {
        SnapHdr h;
        if (esp_partition_read(snapPart, i * kSlotBytes, &h, sizeof(h)) != ESP_OK) continue;
        if (h.magic != kSnapMagic || h.version != kSnapVersion) continue;
        if (h.textLen > kSlotBytes - sizeof(SnapHdr)) continue;
        if (h.crc != crc32(&h, offsetof(SnapHdr, crc))) continue;
        slotSeq[i] = h.seq;
        if (h.seq >= maxSeq) maxSeq = h.seq;
    }
    nextSeq = maxSeq + 1;
    if (nextSeq == 0) nextSeq = 1;
}

/**
 * @brief Return the slot index of the newestIndex-th newest valid snapshot.
 * @param newestIndex  0 = highest seq.
 * @return Slot 0..7, or -1 if not enough snapshots.
 */
int slotForNewest(uint8_t newestIndex) {
    uint32_t upper = 0xFFFFFFFFu;
    int chosen = -1;
    for (uint8_t k = 0; k <= newestIndex; k++) {
        uint32_t best = 0;
        chosen = -1;
        for (int i = 0; i < kSlots; i++) {
            if (slotSeq[i] && slotSeq[i] < upper && slotSeq[i] >= best) {
                best = slotSeq[i];
                chosen = i;
            }
        }
        if (chosen < 0) return -1;
        upper = best;
    }
    return chosen;
}

/**
 * @brief Append bytes to the RTC ring, overwriting the oldest bytes when full.
 * @param s  Source bytes.
 * @param n  Number of bytes to append.
 * @note Caller holds mux.
 */
void rtcPut(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        rtc.data[rtc.head] = s[i];
        rtc.head = (uint16_t)((rtc.head + 1) % kRtcBytes);
        if (rtc.len < kRtcBytes) rtc.len++;
    }
}

/**
 * @brief Append bytes to the PSRAM ring, overwriting the oldest bytes when full.
 * @param s  Source bytes.
 * @param n  Number of bytes to append; no-op if the ring was not allocated.
 * @note Caller holds mux.
 */
void ringPut(const char* s, size_t n) {
    if (!ring) return;
    for (size_t i = 0; i < n; i++) {
        ring[ringHead] = s[i];
        ringHead = (ringHead + 1) % kRingBytes;
    }
    ringLen = (ringLen + n > kRingBytes) ? kRingBytes : ringLen + n;
}
}  // namespace

namespace DiagLog {

void begin() {
    if (rtc.magic == kMagic && rtc.len <= kRtcBytes && rtc.head < kRtcBytes) {
        const size_t start = (rtc.head + kRtcBytes - rtc.len) % kRtcBytes;
        size_t o = 0;
        // Skip the first, possibly cut line so the tail starts at a line boundary.
        bool atLine = rtc.len < kRtcBytes;
        for (size_t i = 0; i < rtc.len; i++) {
            const char c = rtc.data[(start + i) % kRtcBytes];
            if (!atLine) {
                if (c == '\n') atLine = true;
                continue;
            }
            prevTail[o++] = c;
        }
        prevTail[o] = 0;
    }
    memset(&rtc, 0, sizeof(rtc));
    rtc.magic = kMagic;

    void* p = HuMem::allocPreferPsram(kRingBytes);
    if (p && HuMem::ptrInPsram(p)) {
        ring = static_cast<char*>(p);
    } else if (p) {
        HuMem::freePreferPsram(p);  // 64 KiB internal heap is too expensive; RTC ring only
    }
    void* s = HuMem::allocPreferPsram(kSlotBytes);
    if (s && HuMem::ptrInPsram(s)) {
        slotBuf = static_cast<char*>(s);
    } else if (s) {
        HuMem::freePreferPsram(s);
    }
    snapPart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                        "spiffs");
    if (snapPart && snapPart->size < kSlots * kSlotBytes) snapPart = nullptr;
    scanSlots();
    uint8_t saved = 0;
    for (uint8_t i = 0; i < kSlots; i++) {
        if (slotSeq[i]) saved++;
    }
    Serial.printf("[DIAG] ring=%u psram=%d prev_tail=%u flash=%d saved=%u\n",
                  ring ? (unsigned)kRingBytes : 0u, ring ? 1 : 0, (unsigned)strlen(prevTail),
                  snapPart && slotBuf ? 1 : 0, (unsigned)saved);

    const char* rr = PanicTail::resetReason();
    const bool keep = prevTail[0] || strcmp(rr, "panic") == 0 || strstr(rr, "watchdog") != nullptr ||
                      strcmp(rr, "software") == 0 || strcmp(rr, "brownout") == 0;
    if (keep) persist("boot", true);
}

void printf(const char* fmt, ...) {
    char line[kLineMax];
    int n = snprintf(line, sizeof(line), "%lu ", (unsigned long)millis());
    if (n < 0) return;
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    if (m < 0) return;
    size_t len = strnlen(line, sizeof(line));
    if (len == 0 || line[len - 1] != '\n') {
        if (len >= sizeof(line) - 1) len = sizeof(line) - 2;
        line[len++] = '\n';
        line[len] = 0;
    }
    Serial.print(line + n);
    portENTER_CRITICAL(&mux);
    rtcPut(line, len);
    ringPut(line, len);
    portEXIT_CRITICAL(&mux);
}

const char* previousTail() { return prevTail; }

void appendPreviousTail(JsonObject obj, const char* sid) {
    if (!prevTail[0] || !sid || !sid[0]) return;
    if (strncmp(sentForSid, sid, sizeof(sentForSid) - 1) == 0) return;
    obj["diag_tail"] = prevTail;
    snprintf(sentForSid, sizeof(sentForSid), "%s", sid);
}

size_t size() {
    portENTER_CRITICAL(&mux);
    const size_t n = ringLen;
    portEXIT_CRITICAL(&mux);
    return n;
}

size_t copyOut(size_t offset, char* dst, size_t n) {
    if (!ring || !dst) return 0;
    portENTER_CRITICAL(&mux);
    size_t copied = 0;
    if (offset < ringLen) {
        const size_t start = (ringHead + kRingBytes - ringLen) % kRingBytes;
        const size_t avail = ringLen - offset;
        copied = n < avail ? n : avail;
        for (size_t i = 0; i < copied; i++) {
            dst[i] = ring[(start + offset + i) % kRingBytes];
        }
    }
    portEXIT_CRITICAL(&mux);
    return copied;
}

bool persist(const char* reason, bool force) {
    if (!snapPart || !slotBuf) return false;
    portENTER_CRITICAL(&mux);
    const bool busy = persistBusy;
    if (!busy) persistBusy = true;
    portEXIT_CRITICAL(&mux);
    if (busy) return false;
    const unsigned long now = millis();
    unsigned long minGap = 10000;
    if (reason && strcmp(reason, "ckpt") == 0) minGap = 60000;
    if (!force && lastPersistMs != 0 && (now - lastPersistMs) < minGap) {
        persistBusy = false;
        return false;
    }

    char* body = slotBuf + sizeof(SnapHdr);
    const size_t cap = kSlotBytes - sizeof(SnapHdr);
    size_t used = 0;

    char line[192];
    char crumbs[1024];
    PanicTail::formatBreadcrumbs(crumbs, sizeof(crumbs), false);
    snprintf(line, sizeof(line), "# crumbs this boot gen=%u\n",
             (unsigned)PanicTail::bootGeneration());
    appendText(body, cap, used, line);
    appendText(body, cap, used, crumbs);
    // Open ENTER stack (if any) — survives in flash even when RTC/NVS recovery fails.
    {
        char phaseLine[160];
        PhaseGate::formatOpenLine(phaseLine, sizeof(phaseLine));
        appendText(body, cap, used, phaseLine);
    }

    if (reason && strcmp(reason, "boot") == 0 && prevTail[0]) {
        appendText(body, cap, used, "# previous boot RTC tail\n");
        appendText(body, cap, used, prevTail);
        PanicTail::formatBreadcrumbs(crumbs, sizeof(crumbs), true);
        if (crumbs[0]) {
            appendText(body, cap, used, "# crumbs previous boot\n");
            appendText(body, cap, used, crumbs);
        }
    }

    appendText(body, cap, used, "# log\n");
    const size_t logCap = cap > used ? cap - used : 0;
    if (logCap) {
        const size_t total = size();
        const size_t off = total > logCap ? total - logCap : 0;
        used += copyOut(off, body + used, logCap);
    }

    SnapHdr h;
    memset(&h, 0, sizeof(h));
    h.magic = kSnapMagic;
    h.version = kSnapVersion;
    h.textLen = static_cast<uint16_t>(used);
    h.seq = nextSeq;
    h.bootGeneration = PanicTail::bootGeneration();
    h.uptimeMs = (uint32_t)millis();
    snprintf(h.reason, sizeof(h.reason), "%s", reason ? reason : "?");
    snprintf(h.resetReason, sizeof(h.resetReason), "%s", PanicTail::resetReason());
    snprintf(h.restartCause, sizeof(h.restartCause), "%s", PanicTail::restartCause());
    snprintf(h.fw, sizeof(h.fw), "%s", FW_VERSION);
    h.crc = crc32(&h, offsetof(SnapHdr, crc));
    memcpy(slotBuf, &h, sizeof(h));
    if (used < cap) memset(body + used, 0xFF, cap - used);

    const uint8_t slot = static_cast<uint8_t>((h.seq - 1) % kSlots);
    const uint32_t off = slot * kSlotBytes;
    const bool ok = esp_partition_erase_range(snapPart, off, kSlotBytes) == ESP_OK &&
                    writeFlash(off, slotBuf, kSlotBytes);
    if (ok) {
        slotSeq[slot] = h.seq;
        nextSeq = h.seq + 1;
        if (nextSeq == 0) nextSeq = 1;
        lastPersistMs = now;
    }
    persistBusy = false;
    // After the write so this line belongs to the next snapshot.
    printf("[DIAG] persist %s slot=%u seq=%u bytes=%u ok=%d\n", h.reason, (unsigned)slot,
           (unsigned)h.seq, (unsigned)used, ok ? 1 : 0);
    return ok;
}

bool persistAvailable() { return snapPart != nullptr && slotBuf != nullptr; }

uint8_t snapshotCount() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < kSlots; i++) {
        if (slotSeq[i]) n++;
    }
    return n;
}

size_t copySnapshot(uint8_t newestIndex, size_t offset, char* dst, size_t n) {
    if (!snapPart || !dst || n == 0) return 0;
    const int slot = slotForNewest(newestIndex);
    if (slot < 0) return 0;
    SnapHdr h;
    const uint32_t base = static_cast<uint32_t>(slot) * kSlotBytes;
    if (esp_partition_read(snapPart, base, &h, sizeof(h)) != ESP_OK) return 0;
    if (h.magic != kSnapMagic || h.crc != crc32(&h, offsetof(SnapHdr, crc))) return 0;
    if (h.textLen > kSlotBytes - sizeof(SnapHdr)) return 0;

    char head[192];
    const int hdrN = snprintf(
        head, sizeof(head),
        "# snapshot seq=%u reason=%s reset=%s cause=%s gen=%u fw=%s uptime_ms=%u\n",
        (unsigned)h.seq, h.reason, h.resetReason, h.restartCause, (unsigned)h.bootGeneration, h.fw,
        (unsigned)h.uptimeMs);
    if (hdrN < 0) return 0;
    const size_t headLen = static_cast<size_t>(hdrN);
    const size_t total = headLen + h.textLen;
    if (offset >= total) return 0;

    size_t written = 0;
    if (offset < headLen) {
        const size_t fromHead = headLen - offset;
        const size_t take = fromHead < n ? fromHead : n;
        memcpy(dst, head + offset, take);
        written += take;
        offset = headLen;
    }
    if (written < n) {
        const size_t bodyOff = offset - headLen;
        if (bodyOff < h.textLen) {
            const size_t remain = h.textLen - bodyOff;
            const size_t take = remain < (n - written) ? remain : (n - written);
            if (esp_partition_read(snapPart, base + sizeof(SnapHdr) + bodyOff, dst + written,
                                   take) != ESP_OK) {
                return written;
            }
            written += take;
        }
    }
    return written;
}

void appendStatus(JsonObject obj) {
    obj["ring_bytes"] = (uint32_t)size();
    obj["prev_tail_bytes"] = (uint32_t)strlen(prevTail);
    obj["saved"] = snapshotCount();
    obj["persist_ok"] = persistAvailable();
}

}  // namespace DiagLog
