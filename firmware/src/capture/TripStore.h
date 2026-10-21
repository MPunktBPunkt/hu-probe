#pragma once

/**
 * @file TripStore.h
 * @brief Power-loss safe flash journal for drive-mode trips on the raw "trips" partition.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Flash journal for offline captures that must survive power loss (drive mode, 0.3.0).
 *
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
    /**
     * @brief Reason a trip ended, stored in each commit entry.
     *
     * EndOpen means no closing commit was written (reported as "power_loss").
     * @see endName(), endFromReason()
     */
    enum EndCode : uint8_t {
        EndOpen = 0,
        EndMaxDuration = 1,
        EndUnmount = 2,
        EndBootLong = 3,
        EndTimer = 4,
        EndNoHost = 5,
        EndOther = 6
    };

    /** @brief Trip state bits stored in commit entries (closed = no further appends). */
    enum Flags : uint16_t { FlagClosed = 1, FlagUploaded = 2 };

    /**
     * @brief Trip header written once at the start of the trip's first sector.
     *
     * Packed on-flash layout; validated on scan by magic, version, size and the CRC-32 over
     * all bytes before @c crc. Text fields are fixed-size, NUL-terminated C strings.
     */
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

    /** @brief In-RAM index entry for one trip found by scan() or created by startTrip(). */
    struct Trip {
        uint32_t offset = 0;  // header sector in the partition
        uint32_t len = 0;     // committed record bytes
        uint16_t flags = 0;
        uint8_t end = EndOpen;
        uint8_t commits = 0;
        Meta meta{};

        /** @brief True if the newest commit carries FlagClosed (no further appends). */
        bool closed() const { return flags & FlagClosed; }

        /** @brief True if the newest commit carries FlagUploaded. */
        bool uploaded() const { return flags & FlagUploaded; }

        /** @brief True if the trip still has committed bytes that were not uploaded yet. */
        bool pending() const { return !uploaded() && len > 0; }
    };

    /**
     * @brief Return the process-wide singleton (created on first call).
     * @return Reference to the single TripStore instance.
     */
    static TripStore& instance();

    /**
     * @brief Find the partition and scan the journal.
     *
     * Looks up the data partition with subtype 0x40 labelled "trips", rebuilds the trip
     * index and logs partition size, trip count and free space. Idempotent once found.
     *
     * @return true if the partition is available; false without a "trips" partition
     *         (old partition table).
     */
    bool begin();

    /** @brief True once begin() has found the "trips" partition. */
    bool available() const { return part_ != nullptr; }

    /**
     * @brief Erase sectors ahead of the write cursor (boot phase, before USB attach).
     *
     * Erases 4 KiB sectors from the already-erased end up to cursor + @p bytes (clamped to
     * the partition size) so later appends do not stall on erases. Stops early on the time
     * budget or an erase error. No-op without a partition.
     *
     * @param bytes  Number of bytes behind the write cursor that should be erased.
     * @param maxMs  Time budget in milliseconds.
     * @warning Flash erase blocks the flash cache; call before USB attach.
     */
    void preErase(size_t bytes, uint32_t maxMs);

    /**
     * @brief Check whether a new trip may be started.
     * @return true if the partition exists, no trip is being written, fewer than 32 trips are
     *         stored and at least 64 KiB are free.
     */
    bool canStart() const;

    /**
     * @brief Return the bytes available for record data of the next trip.
     * @return Free bytes behind the cursor minus the header sector; 0 without a partition or
     *         when less than one sector remains.
     */
    size_t freeBytes() const;

    /**
     * @brief Write meta at the cursor and make it the current trip (fills tripId).
     *
     * Fills magic, version, size, tripId (from the persistent counter "trip_seq" in NVS
     * namespace "huprobe"), bootGeneration and crc, erases the header plus first data sector
     * and writes the header. Resets the written byte counter.
     *
     * @param[in,out] meta  Trip metadata supplied by the caller; the bookkeeping fields above
     *                      are overwritten.
     * @return true on success; false if canStart() is false or the erase/write failed.
     */
    bool startTrip(Meta& meta);

    /** @brief True while a trip started this boot is open for appends. */
    bool writing() const { return curIdx_ >= 0; }

    /**
     * @brief Append record bytes to the current trip; false when the partition is full.
     *
     * Data becomes durable only after the next commit(). Erases sectors on demand.
     *
     * @param src  Record bytes (may live in PSRAM; copied through an internal bounce buffer).
     * @param n    Number of bytes to append.
     * @return true if written (or @p n is 0 while writing); false if no trip is open, the
     *         partition is full or the flash write failed.
     */
    bool append(const uint8_t* src, size_t n);

    /** @brief Record bytes appended to the current trip (committed or not). */
    uint32_t written() const { return written_; }

    /**
     * @brief Commit everything appended so far; closed = no further appends.
     *
     * Appends a commit entry to the header sector. When @p closed is set, the cursor moves
     * behind the trip's data and the current trip is released, even if the commit write
     * failed.
     *
     * @param end     End reason stored in the commit entry.
     * @param closed  true to close the trip (sets FlagClosed).
     * @return true if the commit entry was written; false if no trip is open, all commit
     *         slots are used or the write failed.
     */
    bool commit(EndCode end, bool closed);

    /**
     * @brief Current trip is worthless (no host): commit as empty so it is skipped.
     *
     * Writes a zero-length commit flagged closed and uploaded with EndNoHost, moves the
     * cursor to the sector after the header and releases the current trip. No-op if no trip
     * is being written.
     */
    void discardCurrent();

    /** @brief tripId of the trip being written, or 0 if none. */
    uint32_t currentTripId() const { return writing() ? trips_[curIdx_].meta.tripId : 0; }

    /** @brief Number of commit entries of the trip being written, or 0 if none. */
    uint8_t currentCommits() const { return writing() ? trips_[curIdx_].commits : 0; }

    /** @brief Number of trips in the index (0..32), including the one being written. */
    uint8_t tripCount() const { return count_; }

    /**
     * @brief Count trips that still need uploading.
     * @return Number of pending trips, excluding the trip currently being written.
     */
    uint8_t pendingCount() const;

    /**
     * @brief Return the trip at an index.
     * @param i  Index in the journal order, 0..tripCount()-1.
     * @return Pointer into the internal index, or nullptr if @p i is out of range.
     */
    const Trip* trip(uint8_t i) const { return i < count_ ? &trips_[i] : nullptr; }

    /**
     * @brief Return the oldest trip that still needs uploading.
     * @return Pointer to the first pending trip (excluding the one being written), or nullptr.
     */
    const Trip* firstPending() const;

    /**
     * @brief Read committed record bytes of a trip.
     * @param t    Trip from trip() or firstPending().
     * @param off  Byte offset within the trip's record data.
     * @param dst  Destination buffer of at least @p n bytes.
     * @param n    Number of bytes to read.
     * @return true on success; false without a partition, if @p off + @p n exceeds the
     *         committed length or the flash read failed.
     */
    bool readData(const Trip& t, uint32_t off, uint8_t* dst, size_t n) const;

    /**
     * @brief Mark a trip as uploaded by appending a commit with FlagClosed | FlagUploaded.
     * @param tripId  Trip identifier from Meta::tripId.
     * @return true if the commit was written; false if the trip is unknown, still being
     *         written, out of commit slots or the write failed.
     */
    bool markUploaded(uint32_t tripId);

    /**
     * @brief All trips uploaded or empty: erase the used sectors and start over at 0.
     * @return true if the journal was erased; false if a trip is open, the journal is empty,
     *         trips are still pending or the erase failed.
     */
    bool resetIfDone();

    /**
     * @brief Erase every used sector (WebUI).
     *
     * Clears the trip index and resets the cursor to 0.
     *
     * @return true if the erase succeeded; false without a partition, while a trip is being
     *         written, or on an erase error.
     * @warning Blocks for the duration of the flash erase.
     */
    bool eraseAll();

    /**
     * @brief Map an end code to its JSON/log name.
     * @param end  EndCode value.
     * @return Static string such as "power_loss" (EndOpen), "max_duration", "unmount",
     *         "boot_long", "timer", "no_host"; "other" for unknown values.
     */
    static const char* endName(uint8_t end);

    /**
     * @brief Map a capture stop reason string to an end code.
     *
     * "umount"/"unplug_idle" map to EndUnmount, "auto_*"/"media_hide_failed" to EndTimer.
     *
     * @param stopReason  Stop reason from the capture; may be nullptr or empty.
     * @return Matching EndCode, EndOther if unknown, nullptr or empty.
     */
    static EndCode endFromReason(const char* stopReason);

    /**
     * @brief Serialize partition status and the trip list.
     *
     * Writes available, partition_bytes, used_bytes, free_bytes, pending, writing,
     * current_trip_id/current_written (while writing) and a "trips" array with trip_id,
     * offset, bytes, commits, closed, uploaded, end_reason, writing, vehicle, scenario,
     * media, wallclock, fw, fine_data.
     *
     * @param obj  JSON object to fill.
     */
    void toJson(JsonObject obj) const;

private:
    /** @brief Private: use instance(). */
    TripStore() = default;

    /**
     * @brief Commit entry appended in the header sector behind the Meta (from offset 256).
     *
     * The newest entry with a valid magic and CRC defines length, flags and end reason.
     */
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

    /**
     * @brief Compute the standard CRC-32 (reflected, polynomial 0xEDB88320).
     * @param data  Bytes to checksum.
     * @param n     Number of bytes.
     * @return CRC-32 value.
     */
    static uint32_t crc32(const void* data, size_t n);

    /**
     * @brief Round a partition offset up to the next 4 KiB sector boundary.
     * @param v  Byte offset.
     * @return @p v rounded up to a multiple of kSector.
     */
    static uint32_t alignUp(uint32_t v) { return (v + kSector - 1) & ~(kSector - 1); }

    /**
     * @brief Rebuild the trip index from flash and set the write cursor.
     *
     * Stops at the first sector without a valid Meta, at the first trip without any valid
     * commit (its sectors are reused) or after 32 trips. Resets the current-trip state.
     */
    void scan();

    /**
     * @brief Make sure all sectors up to an offset are erased this boot.
     * @param end  Partition offset (rounded up to a sector) that must be writable.
     * @return true if erased; false if @p end exceeds the partition or an erase failed.
     */
    bool ensureErased(uint32_t end);

    /**
     * @brief Write bytes to the partition through a 256-byte internal-RAM bounce buffer.
     * @param off  Partition offset; the target range must already be erased.
     * @param src  Source bytes (may live in PSRAM).
     * @param n    Number of bytes.
     * @return true on success, false on the first failed flash write.
     */
    bool writeFlash(uint32_t off, const void* src, size_t n);

    /**
     * @brief Append a commit entry to a trip's header sector and update the in-RAM entry.
     *
     * The committed length is written_ for the trip being written, otherwise the trip's
     * existing length.
     *
     * @param t      Trip to commit.
     * @param flags  Flags bits to store.
     * @param end    EndCode to store.
     * @return true on success; false if all commit slots are used or the write failed.
     */
    bool writeCommit(Trip& t, uint16_t flags, uint8_t end);

    /**
     * @brief Increment and persist the trip counter "trip_seq" in NVS namespace "huprobe".
     * @return New trip id; 1 if NVS could not be opened.
     */
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
