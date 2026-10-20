#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Flash journal for offline captures that must survive power loss (drive mode, 0.3.0).
 * Raw data partition "trips", no file system. Trips follow each other, each one starts
 * on a 4 KiB sector:
 *   sector 0: TripMeta (written once) + Commit entries appended behind it
 *             (flash bits only go 1→0, so appends need no erase; newest valid entry wins)
 *   sector 1…: compact/1 records exactly as in the RAM buffer, append-only
 * Data beyond the last commit is ignored. A trip without any commit (power lost before
 * the first checkpoint) ends the journal and its sectors are reused.
 */
class TripStore {
public:
    enum EndCode : uint8_t {
        EndOpen = 0,
        EndMaxDuration = 1,
        EndUnmount = 2,
        EndBootLong = 3,
        EndTimer = 4,
        EndNoHost = 5,
        EndOther = 6
    };
    enum Flags : uint16_t { FlagClosed = 1, FlagUploaded = 2 };

    struct __attribute__((packed)) Meta {
        uint32_t magic;
        uint16_t version;
        uint16_t size;
        uint32_t tripId;
        uint32_t bootGeneration;
        int64_t tBaseUs;
        int64_t anchorUnixUs;
        int64_t anchorCaptureUs;
        uint32_t capBytes;
        uint32_t measureMs;
        uint8_t fineData;
        uint8_t freeMarkers;
        uint8_t endOnUnmount;
        uint8_t softUsb;
        char vehicle[24];
        char scenario[32];
        char profile[24];
        char wallclock[32];
        char fw[16];
        char endStrategy[16];
        char media[16];
        uint32_t crc;
    };

    struct Trip {
        uint32_t offset = 0;  // header sector in the partition
        uint32_t len = 0;     // committed record bytes
        uint16_t flags = 0;
        uint8_t end = EndOpen;
        uint8_t commits = 0;
        Meta meta{};
        bool closed() const { return flags & FlagClosed; }
        bool uploaded() const { return flags & FlagUploaded; }
        bool pending() const { return !uploaded() && len > 0; }
    };

    static TripStore& instance();

    /** Find the partition and scan the journal. False without a "trips" partition. */
    bool begin();
    bool available() const { return part_ != nullptr; }

    /** Erase sectors ahead of the write cursor (boot phase, before USB attach). */
    void preErase(size_t bytes, uint32_t maxMs);
    bool canStart() const;
    size_t freeBytes() const;

    /** Write meta at the cursor and make it the current trip (fills tripId). */
    bool startTrip(Meta& meta);
    bool writing() const { return curIdx_ >= 0; }
    /** Append record bytes to the current trip; false when the partition is full. */
    bool append(const uint8_t* src, size_t n);
    uint32_t written() const { return written_; }
    /** Commit everything appended so far; closed = no further appends. */
    bool commit(EndCode end, bool closed);
    /** Current trip is worthless (no host): commit as empty so it is skipped. */
    void discardCurrent();
    uint32_t currentTripId() const { return writing() ? trips_[curIdx_].meta.tripId : 0; }
    uint8_t currentCommits() const { return writing() ? trips_[curIdx_].commits : 0; }

    uint8_t tripCount() const { return count_; }
    uint8_t pendingCount() const;
    const Trip* trip(uint8_t i) const { return i < count_ ? &trips_[i] : nullptr; }
    const Trip* firstPending() const;
    bool readData(const Trip& t, uint32_t off, uint8_t* dst, size_t n) const;
    bool markUploaded(uint32_t tripId);
    /** All trips uploaded or empty: erase the used sectors and start over at 0. */
    bool resetIfDone();
    /** Erase every used sector (WebUI). */
    bool eraseAll();

    static const char* endName(uint8_t end);
    static EndCode endFromReason(const char* stopReason);
    void toJson(JsonObject obj) const;

private:
    TripStore() = default;

    struct __attribute__((packed)) Commit {
        uint32_t magic;
        uint32_t len;
        uint16_t flags;
        uint8_t end;
        uint8_t seq;
        uint32_t crc;
    };

    static constexpr uint32_t kMetaMagic = 0x48555452;    // "HUTR"
    static constexpr uint32_t kCommitMagic = 0x4855434D;  // "HUCM"
    static constexpr uint16_t kVersion = 1;
    static constexpr uint32_t kSector = 4096;
    static constexpr uint32_t kCommitBase = 256;
    static constexpr uint8_t kMaxCommits = (kSector - kCommitBase) / sizeof(Commit);
    static constexpr uint8_t kMaxTrips = 32;
    static constexpr size_t kMinFree = 64 * 1024;

    static uint32_t crc32(const void* data, size_t n);
    static uint32_t alignUp(uint32_t v) { return (v + kSector - 1) & ~(kSector - 1); }
    void scan();
    bool ensureErased(uint32_t end);
    bool writeFlash(uint32_t off, const void* src, size_t n);
    bool writeCommit(Trip& t, uint16_t flags, uint8_t end);
    uint32_t nextTripId();

    const void* part_ = nullptr;  // esp_partition_t*
    uint32_t size_ = 0;
    uint32_t cursor_ = 0;         // next free sector (start of the next trip)
    uint32_t erasedEnd_ = 0;      // sectors in [cursor_, erasedEnd_) are erased this boot
    Trip trips_[kMaxTrips];
    uint8_t count_ = 0;
    int8_t curIdx_ = -1;  // trip being written this boot
    uint32_t written_ = 0;
};
