#include "core/DiagLog.h"
#include "core/HuMem.h"

#include <esp_attr.h>
#include <cstdarg>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x48554C47;  // HULG
constexpr size_t kRtcBytes = 2048;
constexpr size_t kRingBytes = 64 * 1024;
constexpr size_t kLineMax = 192;

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

void rtcPut(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        rtc.data[rtc.head] = s[i];
        rtc.head = (uint16_t)((rtc.head + 1) % kRtcBytes);
        if (rtc.len < kRtcBytes) rtc.len++;
    }
}

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
    Serial.printf("[DIAG] ring=%u psram=%d prev_tail=%u\n", ring ? (unsigned)kRingBytes : 0u,
                  ring ? 1 : 0, (unsigned)strlen(prevTail));
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

}  // namespace DiagLog
