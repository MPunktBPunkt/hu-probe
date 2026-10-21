/**
 * @file TripStore.cpp
 * @brief Implementation of the drive-mode trip journal on the raw "trips" flash partition.
 */

#include "capture/TripStore.h"
#include "core/DiagLog.h"
#include "core/PanicTail.h"
#include <Preferences.h>
#include <cstring>
#include <esp_partition.h>

namespace {
/**
 * @brief Cast the opaque partition handle stored in the header back to esp_partition_t.
 * @param p  Value of TripStore::part_.
 * @return Typed partition pointer (may be nullptr).
 */
const esp_partition_t* P(const void* p) { return static_cast<const esp_partition_t*>(p); }
}  // namespace

TripStore& TripStore::instance() {
    static TripStore s;
    return s;
}

uint32_t TripStore::crc32(const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

bool TripStore::begin() {
    if (part_) return true;
    const esp_partition_t* p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(0x40), "trips");
    part_ = p;
    if (!p) {
        DiagLog::printf("[TRIP] no 'trips' partition (old partition table?)\n");
        return false;
    }
    size_ = p->size;
    scan();
    erasedEnd_ = cursor_;
    DiagLog::printf("[TRIP] partition %u KiB, %u trips (%u pending), free %u KiB\n",
                    (unsigned)(size_ / 1024), (unsigned)count_, (unsigned)pendingCount(),
                    (unsigned)(freeBytes() / 1024));
    return true;
}

void TripStore::scan() {
    count_ = 0;
    curIdx_ = -1;
    written_ = 0;
    uint32_t off = 0;
    while (off + kSector <= size_ && count_ < kMaxTrips) {
        Trip t;
        t.offset = off;
        if (esp_partition_read(P(part_), off, &t.meta, sizeof(t.meta)) != ESP_OK) break;
        if (t.meta.magic != kMetaMagic || t.meta.version != kVersion ||
            t.meta.size != sizeof(Meta) ||
            t.meta.crc != crc32(&t.meta, offsetof(Meta, crc))) {
            break;
        }
        for (uint8_t i = 0; i < kMaxCommits; i++) {
            Commit c;
            if (esp_partition_read(P(part_), off + kCommitBase + i * sizeof(Commit), &c,
                                   sizeof(c)) != ESP_OK) {
                break;
            }
            if (c.magic == 0xFFFFFFFFu) break;
            if (c.magic != kCommitMagic || c.crc != crc32(&c, offsetof(Commit, crc))) continue;
            if (c.len > size_ - off - kSector) continue;
            t.len = c.len;
            t.flags = c.flags;
            t.end = c.end;
            t.commits = i + 1;
        }
        if (t.commits == 0) break;  // never committed: reuse from here
        trips_[count_++] = t;
        off = alignUp(off + kSector + t.len);
    }
    cursor_ = off;
}

bool TripStore::ensureErased(uint32_t end) {
    end = alignUp(end);
    if (end > size_) return false;
    while (erasedEnd_ < end) {
        if (esp_partition_erase_range(P(part_), erasedEnd_, kSector) != ESP_OK) return false;
        erasedEnd_ += kSector;
    }
    return true;
}

bool TripStore::writeFlash(uint32_t off, const void* src, size_t n) {
    // Records live in PSRAM; bounce through internal RAM for the SPI flash driver.
    uint8_t bounce[256];
    const uint8_t* s = static_cast<const uint8_t*>(src);
    while (n) {
        const size_t k = n < sizeof(bounce) ? n : sizeof(bounce);
        memcpy(bounce, s, k);
        if (esp_partition_write(P(part_), off, bounce, k) != ESP_OK) return false;
        off += k;
        s += k;
        n -= k;
    }
    return true;
}

void TripStore::preErase(size_t bytes, uint32_t maxMs) {
    if (!part_) return;
    const unsigned long t0 = millis();
    uint32_t target = alignUp(cursor_ + bytes);
    if (target > size_) target = size_;
    while (erasedEnd_ < target && millis() - t0 < maxMs) {
        if (!ensureErased(erasedEnd_ + kSector)) break;
    }
    DiagLog::printf("[TRIP] pre-erased up to %u KiB (cursor %u KiB) in %lu ms\n",
                    (unsigned)(erasedEnd_ / 1024), (unsigned)(cursor_ / 1024),
                    (unsigned long)(millis() - t0));
}

size_t TripStore::freeBytes() const {
    if (!part_ || cursor_ + kSector >= size_) return 0;
    return size_ - cursor_ - kSector;
}

bool TripStore::canStart() const {
    return part_ && !writing() && count_ < kMaxTrips && freeBytes() >= kMinFree;
}

uint32_t TripStore::nextTripId() {
    Preferences prefs;
    uint32_t id = 1;
    if (prefs.begin("huprobe", false)) {
        id = prefs.getUInt("trip_seq", 0) + 1;
        prefs.putUInt("trip_seq", id);
        prefs.end();
    }
    return id;
}

bool TripStore::startTrip(Meta& meta) {
    if (!canStart()) return false;
    meta.magic = kMetaMagic;
    meta.version = kVersion;
    meta.size = sizeof(Meta);
    meta.tripId = nextTripId();
    meta.bootGeneration = PanicTail::bootGeneration();
    meta.crc = crc32(&meta, offsetof(Meta, crc));
    if (!ensureErased(cursor_ + 2 * kSector) || !writeFlash(cursor_, &meta, sizeof(meta))) {
        DiagLog::printf("[TRIP] start failed at %u\n", (unsigned)cursor_);
        return false;
    }
    Trip& t = trips_[count_];
    t = Trip();
    t.offset = cursor_;
    t.meta = meta;
    curIdx_ = (int8_t)count_;
    count_++;
    written_ = 0;
    DiagLog::printf("[TRIP] start #%u at %u KiB\n", (unsigned)meta.tripId,
                    (unsigned)(cursor_ / 1024));
    return true;
}

bool TripStore::append(const uint8_t* src, size_t n) {
    if (!writing() || n == 0) return writing();
    Trip& t = trips_[curIdx_];
    const uint32_t off = t.offset + kSector + written_;
    if (off + n > size_ || !ensureErased(off + n)) return false;
    if (!writeFlash(off, src, n)) return false;
    written_ += n;
    return true;
}

bool TripStore::writeCommit(Trip& t, uint16_t flags, uint8_t end) {
    if (t.commits >= kMaxCommits) return false;
    Commit c;
    c.magic = kCommitMagic;
    c.len = (writing() && &t == &trips_[curIdx_]) ? written_ : t.len;
    c.flags = flags;
    c.end = end;
    c.seq = t.commits;
    c.crc = crc32(&c, offsetof(Commit, crc));
    if (!writeFlash(t.offset + kCommitBase + t.commits * sizeof(Commit), &c, sizeof(c))) {
        return false;
    }
    t.len = c.len;
    t.flags = flags;
    t.end = end;
    t.commits++;
    return true;
}

bool TripStore::commit(EndCode end, bool closed) {
    if (!writing()) return false;
    Trip& t = trips_[curIdx_];
    const bool ok = writeCommit(t, closed ? FlagClosed : 0, end);
    if (closed) {
        cursor_ = alignUp(t.offset + kSector + t.len);
        if (erasedEnd_ < cursor_) erasedEnd_ = cursor_;
        curIdx_ = -1;
        DiagLog::printf("[TRIP] closed #%u len=%u end=%s commits=%u\n",
                        (unsigned)t.meta.tripId, (unsigned)t.len, endName(end),
                        (unsigned)t.commits);
    }
    return ok;
}

void TripStore::discardCurrent() {
    if (!writing()) return;
    Trip& t = trips_[curIdx_];
    written_ = 0;
    writeCommit(t, FlagClosed | FlagUploaded, EndNoHost);
    cursor_ = alignUp(t.offset + kSector);
    if (erasedEnd_ < cursor_) erasedEnd_ = cursor_;
    curIdx_ = -1;
}

uint8_t TripStore::pendingCount() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count_; i++) {
        if ((int8_t)i != curIdx_ && trips_[i].pending()) n++;
    }
    return n;
}

const TripStore::Trip* TripStore::firstPending() const {
    for (uint8_t i = 0; i < count_; i++) {
        if ((int8_t)i != curIdx_ && trips_[i].pending()) return &trips_[i];
    }
    return nullptr;
}

bool TripStore::readData(const Trip& t, uint32_t off, uint8_t* dst, size_t n) const {
    if (!part_ || off + n > t.len) return false;
    return esp_partition_read(P(part_), t.offset + kSector + off, dst, n) == ESP_OK;
}

bool TripStore::markUploaded(uint32_t tripId) {
    for (uint8_t i = 0; i < count_; i++) {
        Trip& t = trips_[i];
        if (t.meta.tripId != tripId) continue;
        if ((int8_t)i == curIdx_) return false;  // still being written
        return writeCommit(t, t.flags | FlagClosed | FlagUploaded, t.end);
    }
    return false;
}

bool TripStore::resetIfDone() {
    if (!part_ || writing() || count_ == 0) return false;
    if (pendingCount() != 0) return false;
    return eraseAll();
}

bool TripStore::eraseAll() {
    if (!part_ || writing()) return false;
    const unsigned long t0 = millis();
    const uint32_t used = alignUp(cursor_ > erasedEnd_ ? cursor_ : erasedEnd_);
    const bool ok = esp_partition_erase_range(P(part_), 0, used ? used : kSector) == ESP_OK;
    count_ = 0;
    cursor_ = 0;
    erasedEnd_ = ok ? (used ? used : kSector) : 0;
    DiagLog::printf("[TRIP] erased %u KiB in %lu ms ok=%d\n", (unsigned)(used / 1024),
                    (unsigned long)(millis() - t0), ok ? 1 : 0);
    return ok;
}

const char* TripStore::endName(uint8_t end) {
    switch (end) {
        case EndOpen: return "power_loss";
        case EndMaxDuration: return "max_duration";
        case EndUnmount: return "unmount";
        case EndBootLong: return "boot_long";
        case EndTimer: return "timer";
        case EndNoHost: return "no_host";
        default: return "other";
    }
}

TripStore::EndCode TripStore::endFromReason(const char* r) {
    if (!r || !r[0]) return EndOther;
    if (!strcmp(r, "max_duration")) return EndMaxDuration;
    if (!strcmp(r, "umount") || !strcmp(r, "unplug_idle")) return EndUnmount;
    if (!strcmp(r, "boot_long")) return EndBootLong;
    if (!strncmp(r, "auto_", 5) || !strcmp(r, "media_hide_failed")) return EndTimer;
    if (!strcmp(r, "no_host")) return EndNoHost;
    return EndOther;
}

void TripStore::toJson(JsonObject obj) const {
    obj["available"] = available();
    if (!part_) return;
    obj["partition_bytes"] = size_;
    obj["used_bytes"] = cursor_;
    obj["free_bytes"] = (uint32_t)freeBytes();
    obj["pending"] = pendingCount();
    obj["writing"] = writing();
    if (writing()) {
        obj["current_trip_id"] = trips_[curIdx_].meta.tripId;
        obj["current_written"] = written_;
    }
    JsonArray arr = obj["trips"].to<JsonArray>();
    for (uint8_t i = 0; i < count_; i++) {
        const Trip& t = trips_[i];
        JsonObject o = arr.add<JsonObject>();
        o["trip_id"] = t.meta.tripId;
        o["offset"] = t.offset;
        o["bytes"] = t.len;
        o["commits"] = t.commits;
        o["closed"] = t.closed();
        o["uploaded"] = t.uploaded();
        o["end_reason"] = t.closed() ? endName(t.end) : "power_loss";
        o["writing"] = (int8_t)i == curIdx_;
        o["vehicle"] = t.meta.vehicle;
        o["scenario"] = t.meta.scenario;
        o["media"] = t.meta.media;
        o["wallclock"] = t.meta.wallclock;
        o["fw"] = t.meta.fw;
        o["fine_data"] = t.meta.fineData != 0;
    }
}
