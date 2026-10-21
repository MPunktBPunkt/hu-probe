#pragma once

/**
 * @file OfflineCapture.h
 * @brief RAM-buffered offline capture of USB/SCSI activity while WiFi is off, replayed later as JSONL.
 */

#include <Arduino.h>
#include "msc/ReadInstrument.h"
#include "session/Session.h"
#include "msc/MscGadget.h"
#include "net/TcpStreamer.h"

/**
 * @brief RAM offline capture (Messkonzept): buffer USB/BOT/SCSI events while WiFi is off, then replay them.
 *
 * WiFi OFF during measure → compact Pending records in heap → WiFi ON → replay JSONL.
 * No flash writes in the measure window (avoids cache stalls) — except drive mode
 * (setPersist), which checkpoints to TripStore in USB idle gaps and marks each one.
 *
 * 0.2.25: experiment marker sequence (BOOT short), capture budget, last-run integrity.
 *
 * @note Singleton (instance()); driven from the Arduino loop task via poll().
 */
class OfflineCapture {
public:
    /** @brief Capture lifecycle; a run goes Idle → Arming → Capturing → Finalizing → Uploading → Idle. */
    enum class State : uint8_t {
        Idle = 0,     ///< No run; no buffer allocated.
        Arming,       ///< arm() done: live session stopped, WiFi off, buffer allocated.
        Capturing,    ///< Measure window open; BOT events go into the RAM buffer.
        Finalizing,   ///< Stop requested; final persist and media_hide in progress.
        Uploading     ///< WiFi back on; records are replayed to the collector.
    };

    /**
     * @brief Return the process-wide singleton (function-local static, created on first call).
     * @return Reference to the single OfflineCapture instance.
     */
    static OfflineCapture& instance();

    /** @brief Current lifecycle state. */
    State state() const { return state_; }

    /** @brief True from arm() until the upload has finished (Arming, Capturing, Finalizing or Uploading). */
    bool active() const {
        return state_ == State::Arming || state_ == State::Capturing ||
               state_ == State::Finalizing || state_ == State::Uploading;
    }

    /** @brief True only while the measure window is open (State::Capturing). */
    bool capturing() const { return state_ == State::Capturing; }

    /** @brief True while a run is active(): the App WiFi supervisor must not touch WiFi then. */
    bool blocksWifiSupervise() const { return active(); }

    /** @brief Capture-window records in the buffer (post-finalize records not included). */
    uint32_t recordCount() const { return recordCount_; }

    /** @brief Records stored after finalize (media hidden, before replay), at most kMaxPostFinalizeRecs. */
    uint32_t postFinalizeRecords() const { return postFinalizeRecords_; }

    /** @brief Capture-window records dropped because the buffer was full. */
    uint32_t overflow() const { return overflow_; }

    /** @brief Bytes of the record buffer filled so far. */
    size_t bytesUsed() const { return used_; }

    /** @brief Allocated record buffer size in bytes (0 when no buffer is allocated). */
    size_t bytesCap() const { return cap_; }

    /** @brief Capture start as UTC "YYYY-MM-DDThh:mm:ssZ", or "boot+<ms>" when no NTP time was set. */
    const char* wallclock() const { return wallclock_; }

    /** @brief Lower-case state name for status JSON ("idle", "arming", "capturing", "finalizing", "uploading"). */
    const char* stateName() const;

    /** @brief Typical record size in bytes (records are variable length). Used for capture_budget. */
    static constexpr size_t recordSize() { return kTypicalRecBytes; }

    /** @brief Default buffer size without PSRAM and minimum accepted size from internal heap: 64 KiB. */
    static constexpr size_t defaultCapBytes() { return kDefaultCap; }

    /** @brief Fallback buffer size reported in capture_budget (48 KiB, heap fragmentation case). */
    static constexpr size_t fallbackCapBytes() { return 48 * 1024; }

    /** @brief Hard capture duration limit in ms (30 min; the int32 µs record time base allows ±35 min). */
    static constexpr uint32_t maxCaptureMs() { return kMaxCaptureMs; }

    /**
     * @brief Estimate how many records fit into a buffer, based on the typical record size.
     * @param capBytes  Buffer size in bytes.
     * @return capBytes / recordSize() (rough; real records are 7..64 bytes).
     */
    static size_t maxRecordsForCap(size_t capBytes) {
        return capBytes / kTypicalRecBytes;
    }

    /**
     * @brief Return the buffer size allocBuffer() would get now.
     *
     * Largest PSRAM step (1 MiB, 768/512/256/128/64 KiB) that fits into the largest free
     * PSRAM block, else the default.
     *
     * @return Expected buffer size in bytes.
     */
    static size_t expectedCapBytes();

    /**
     * @brief Return the rough buffer fill rate used for duration estimates.
     *
     * Fine mode assumes ~64 KiB/s average reads (16 DATA records/s).
     *
     * @param fine  true for fineData mode (one DATA record per 4 KiB chunk).
     * @return Estimated bytes per second (80 normal, 480 fine).
     */
    static uint32_t estBytesPerS(bool fine);

    /**
     * @brief Estimate how long a buffer lasts: capBytes / estBytesPerS, capped by maxCaptureMs.
     * @param capBytes  Buffer size in bytes.
     * @param fine      true for fineData mode.
     * @return Estimated capture duration in seconds (at most 1800).
     */
    static uint32_t estDurationS(size_t capBytes, bool fine);

    /**
     * @brief Arm the offline capture (UI/API): stop live session, grab wallclock, WiFi OFF, alloc buffer.
     *
     * Also clears the EventBus to free heap before the large allocation and records the
     * UTC/capture-clock anchor. Blocks for roughly 1 s (settling delays).
     *
     * @param session  Live session to stop now and to restart for the replay.
     * @param msc      MSC gadget used for re-enumeration, media_hide and soft-detach.
     * @param inst     Instrument that re-emits the replayed events.
     * @param tcp      TCP streamer pumped during the upload.
     * @return true if armed (state Arming); false if a run is already active, the buffer
     *         allocation failed or WiFi could not be stopped (state back to Idle).
     */
    bool arm(Session* session, MscGadget* msc, ReadInstrument* inst, TcpStreamer* tcp);

    /**
     * @brief Open the measure window (Arming → Capturing).
     *
     * After arm: enter Capturing. If softUsb, D+ re-enum after WiFi stop.
     * If autoDetachMs>0, end using endStrategy (media_hide default, soft_detach legacy).
     * markers/markerCount: BOOT-short cycles these names offline (experiment profile).
     * fineData: one DATA record per 4 KiB callback instead of one per CBW (chunk timing).
     * With setPersist active, the TripStore trip is started here, before the re-enum.
     *
     * @param vehicle       Vehicle name for the session/trip metadata.
     * @param scenario      Scenario name for the session/trip metadata.
     * @param profileId     Profile id ("lab_debian" keeps USB detached after the upload).
     * @param softUsb       Soft re-enumerate (D+ detach/attach) at start; umount during it is ignored.
     * @param autoDetachMs  Timed end after this many ms; 0 = no timed end.
     * @param endStrategy   "soft_detach" for the legacy detach, anything else means "media_hide".
     * @param markers       Experiment marker names cycled by BOOT short presses; nullptr = default
     *                      sequence (play_start, track_next, audible_change, replay, stop).
     * @param markerCount   Number of entries in markers (at most 8 are used).
     * @param endOnUnmount  Host umount / unplug ends the capture.
     * @param freeMarkers   BOOT short press writes mark_NN instead of the sequence, double press
     *                      soft-replugs.
     * @param fineData      One DATA record per 4 KiB callback instead of one coalesced record per CBW.
     * @return true if capturing; false if not armed (state is not Arming).
     */
    bool beginCapture(const String& vehicle, const String& scenario, const String& profileId,
                      bool softUsb = true, uint32_t autoDetachMs = 0,
                      const String& endStrategy = "media_hide",
                      const char* const* markers = nullptr, uint8_t markerCount = 0,
                      bool endOnUnmount = true, bool freeMarkers = false,
                      bool fineData = false);

    /**
     * @brief Enable or disable drive mode (flash persistence) for the next run.
     *
     * Drive mode: the next beginCapture also writes the run to the TripStore flash journal
     * (checkpoints in USB idle gaps), so it survives power loss. Cleared after each run.
     *
     * @param on     Requested state; stays off if the TripStore is not available.
     * @param media  Media variant name stored in the trip metadata (max 15 chars); nullptr = "".
     */
    void setPersist(bool on, const char* media = nullptr);

    /** @brief True if drive mode is armed for the next/current run (see setPersist()). */
    bool persisting() const { return persist_; }

    /**
     * @brief Set the optional WiFi connect used by the upload (drive mode: home WLAN or hotspot).
     * @param connect  Blocking connect callback (called with a 25000 ms timeout); nullptr =
     *                 WiFi.begin() with the NVS credentials.
     */
    void setWifiConnector(bool (*connect)(uint32_t timeoutMs)) { wifiConnect_ = connect; }

    /**
     * @brief Upload the oldest stored trip; the upload connects WiFi itself if it is down.
     *
     * Stops the live session, loads the trip from the TripStore into a new buffer (torn
     * tail cut), restores its metadata and enters Uploading; poll() does the replay. An
     * unclosed trip is reported with end reason "power_loss".
     *
     * @param session  Session used for the replay.
     * @param msc      MSC gadget (identity serial / volume label for the session).
     * @param inst     Instrument that re-emits the replayed events.
     * @param tcp      TCP streamer pumped during the upload.
     * @return true if the trip was loaded and the upload started; false if a run is active,
     *         no trip is pending, or the buffer allocation or flash read failed.
     */
    bool replayStored(Session* session, MscGadget* msc, ReadInstrument* inst, TcpStreamer* tcp);

    /**
     * @brief Capture started but no host enumerated: drop the run and the trip, back to Idle.
     *
     * No-op unless capturing. Sets stop reason / upload status "no_host" and frees the buffer.
     */
    void abortNoHost();

    /**
     * @brief End capture (button / unplug+idle / max duration / auto soft-detach).
     *
     * Only sets a flag; the next poll() finalizes. No-op unless capturing.
     *
     * @param reason  Stop reason for status/health JSON and the trip end; nullptr = "stop".
     */
    void requestStop(const char* reason);

    /**
     * @brief App loop: auto-stop, BOOT marker/stop, upload when requested.
     *
     * BOOT button (GPIO0): short press = marker (or free marker / double press = soft
     * replug), long press ≥1.5 s = stop. While capturing also handles the timed end,
     * unplug idle, max duration and the drive-mode checkpoints; in Uploading it advances
     * the upload state machine.
     *
     * @param msc  MSC gadget for plug state, media_hide and soft-detach.
     * @note Call from the Arduino loop task; some phases block (wifiOn up to 25 s).
     */
    void poll(MscGadget* msc);

    /**
     * @brief Tell whether BOT events must go into the RAM buffer instead of ReadInstrument.
     *
     * True while drain must route BOT events into RAM: the capture window plus the
     * post-finalize gap (media hidden, WiFi/session not yet up) so host commands after
     * media_hide (e.g. tags 123–130 in 075344) are not lost before replay starts.
     *
     * @return true while capturing, or while finalizing/uploading before the replay session
     *         started; always false without buffer or during a stored-trip replay.
     */
    bool acceptsPending() const {
        if (!buf_ || replayingStored_) return false;
        if (state_ == State::Capturing) return true;
        return (state_ == State::Finalizing || state_ == State::Uploading) && !sessionStarted_;
    }

    /**
     * @brief Store one BOT/USB event as a compact record (MscBotTap::drain calls this instead of ReadInstrument when acceptsPending()).
     *
     * Outside the capture window the record is flagged post-finalize (diagnostic tail, not
     * counted against integrity). Unless fineData, a DATA chunk that continues the previous
     * DATA record (same bot tag, contiguous LBA, same post flag) is coalesced into it. A
     * host umount (kind 8) requests stop "umount" when endOnUnmount is set and the soft
     * re-enum grace is over.
     *
     * @param kind           MscBotTap::Kind: 1 CBW, 2 DATA, 3 CSW, 4 need-reset, 5 control,
     *                       6 set_address, 7 mount, 8 umount, 9 ep_stall, 10 ep_clear_halt,
     *                       11 msc_bot_reset.
     * @param tUs            Event time on the capture clock (ProbeClock µs); must be within
     *                       int32 µs of the buffer base.
     * @param raw31          Raw 31-byte CBW (kind 1) or 13-byte CSW (kind 3); unused otherwise.
     * @param botTag         DATA: BOT tag of the owning CBW.
     * @param lba            DATA: first LBA of the chunk (512-byte blocks).
     * @param nBytes         DATA: chunk length in bytes.
     * @param durUs          DATA: chunk duration in µs.
     * @param gapUs          DATA: gap_us value of the chunk in µs.
     * @param wValue         Control (5) wValue; kinds 6/7/8: the wValue stored for the epoch event.
     * @param wIndex         Control (5): wIndex.
     * @param wLength        Control (5): wLength.
     * @param bmRequestType  Control (5): bmRequestType.
     * @param bRequest       Control (5): bRequest.
     * @param epAddr         Kinds 9/10: endpoint address.
     * @param flag           Event flag, low 7 bits stored in the header (used by replay for
     *                       control and ep_clear_halt).
     * @param tag            DATA: media region tag.
     * @param kindStr        DATA: media region kind.
     * @param dir            DATA: transfer direction ("in"/"out"); tag/kindStr/dir are stored as a
     *                       region code if they match a MediaLayout region and dir is "in",
     *                       else inline (truncated to 7/7/3 chars).
     * @return true if stored or coalesced; false if not accepting, unknown kind, raw31
     *         missing, or the record was dropped (buffer full, time out of range).
     */
    bool appendPending(uint8_t kind, int64_t tUs, const uint8_t* raw31, uint32_t botTag,
                       uint32_t lba, uint32_t nBytes, uint32_t durUs, uint32_t gapUs,
                       uint16_t wValue, uint16_t wIndex, uint16_t wLength,
                       uint8_t bmRequestType, uint8_t bRequest, uint8_t epAddr, uint8_t flag,
                       const char* tag, const char* kindStr, const char* dir);

    /**
     * @brief Store an operator marker at the current capture time (no-op unless capturing).
     * @param name  Marker name (max 15 chars kept); nullptr or "" = "operator".
     */
    void queueMarker(const char* name);

    /**
     * @brief Protocol-derived marker (AutoMarker) at Capture-tUs; also in the post-finalize gap.
     * @param tUs   Capture time of the triggering command (ProbeClock µs).
     * @param name  Marker name (max 15 chars kept).
     */
    void appendAutoMarker(int64_t tUs, const char* name);

    /** @brief Overflow of the last finished run (SequenceRunner splits the step); 0 if no run yet. */
    uint32_t lastRunOverflow() const { return lastRun_.valid ? lastRun_.overflow : 0; }

    /** @brief True if the last finished run passed the probe-side integrity gate (no overflow/drops, complete). */
    bool lastRunIntegrity() const { return lastRun_.valid && lastRun_.integrityPass; }

    /** @brief Current upload phase string (e.g. "replaying", "queued", "delivered"). */
    const char* uploadStatus() const { return uploadStatus_.c_str(); }

    /** @brief Trip id being uploaded (0 if none). */
    uint32_t currentTripId() const { return tripId_; }

    /** @brief True while a TripStore flash trip is being replayed to the collector. */
    bool replayingStored() const { return replayingStored_; }

    /**
     * @brief Fill the offline-capture status: state, counters, end settings, markers,
     *        capture_budget and last_run (if any).
     * @param obj  JSON object to fill.
     */
    void toStatusJson(JsonObject obj) const;

    /**
     * @brief Fill capture_budget object (theoretical 64 KiB target + actual if armed).
     *
     * Capacity is the allocated size once armed, else expectedCapBytes(); elapsed/remaining
     * ms are added while capturing with a timed end.
     *
     * @param obj  JSON object to fill.
     */
    void toBudgetJson(JsonObject obj) const;

private:
    /** @brief Private: use instance(). */
    OfflineCapture() = default;

    /**
     * @brief Allocate the record buffer and reset all buffer/replay counters.
     *
     * Tries the PSRAM steps (1 MiB down to 64 KiB) first, an internal-heap result is kept
     * only if ≥ 64 KiB; then plain malloc 64/48/32/24 KiB (fragmentation after long
     * tcp_live sessions). Sets the record time base tBaseUs_ to now.
     *
     * @return true if a buffer was allocated; false if every size failed (cap_ = 0).
     */
    bool allocBuffer();

    /** @brief Free the record buffer (PSRAM or heap) and zero its size and fill level. */
    void freeBuffer();

    /**
     * @brief Stop WiFi softly (disconnect + esp_wifi_stop, no mode switch).
     *
     * Soft stop — WIFI_OFF/mode switch correlated with field reboots under OTG. Blocks ~130 ms.
     *
     * @return Always true (the esp_wifi_stop error is only logged).
     */
    bool wifiOff();

    /**
     * @brief Restart WiFi in STA mode (no power save, 19.5 dBm) and connect.
     *
     * Uses the setWifiConnector() callback if set, else WiFi.begin() with NVS creds.
     * Stack AutoReconnect stays off: App::superviseWifi owns reconnect (B28).
     *
     * @return true if connected within 25 s.
     * @warning Blocks up to ~25 s.
     */
    bool wifiOn();

    /**
     * @brief Close the capture window and start the upload (Capturing → Finalizing → Uploading).
     *
     * Drains pending capture-window events while still Capturing, writes the final trip
     * persist, then quiets USB via media_hide (soft-detach only if hiding fails). Later
     * host traffic is tagged post_finalize.
     *
     * @param msc  MSC gadget; nullptr keeps the one from arm().
     */
    void beginFinalize(MscGadget* msc);

    /**
     * @brief Advance the upload state machine one step (Wifi → Session → Replay → Health → Drain → Done).
     *
     * Replay applies backpressure (at most kReplayMaxPending queued events, 10 records per
     * pass). Drive mode without WiFi or collector leaves the trip in flash ("stored_in_flash").
     */
    void pollUpload();

    /**
     * @brief Validate and re-emit the next buffered record through ReadInstrument / Session.
     *
     * B7: replayed events carry the unclamped capture time and capture_seq (buffer index);
     * post-finalize records are tagged "post_finalize_buffered".
     *
     * @return true if a record was replayed; false at the end of the buffer, on a malformed
     *         record (replayError_ set via replayFail()) or without session/instrument.
     */
    bool replayOneRec();

    /**
     * @brief Publish the capture_health footer event (counters, replay result, trip/persist
     *        info, heap, previous panic/diag tail).
     * @param final  true for the closing footer; only then can "complete"/"integrity_pass" be true.
     */
    void publishOfflineHealth(bool final);

    /**
     * @brief Pump TCP until the EventBus is empty and connected, or until the timeout.
     * @param maxMs  Maximum blocking time in ms.
     * @return true if no events are pending afterwards.
     */
    bool drainUpload(uint32_t maxMs);

    /**
     * @brief Undo media_hide / fallback soft-detach after the upload.
     *
     * Lab: never restore media / soft-attach after offline — that reopens OTG+WiFi coex
     * (restoreMedia-before-detach was the T3b killer). Stay soft-detached.
     */
    void restoreUsbAfterUpload();

    /** @brief True if vehicle or profile is "lab_debian" (lab host: keep USB detached around WiFi). */
    bool isLabDebian() const;

    /**
     * @brief Lab only: soft-detach USB (cancels a softCycle auto-reattach) so it stays detached.
     * @param why  Reason for the log line; nullptr = "?".
     */
    void ensureLabDetached(const char* why);

    /**
     * @brief Append a capture-window marker record (kind 100).
     * @param tUs   Marker time on the capture clock (ProbeClock µs).
     * @param name  Marker name (max 15 chars kept); nullptr = empty name.
     */
    void appendMarkerRec(int64_t tUs, const char* name);

    /**
     * @brief Space for one record of n bytes, or nullptr (counts overflow / post-finalize drop).
     *
     * Never stores a clamped time: out-of-range tUs is dropped and counted in
     * timeRangeDropped_ (fails integrity). Post-finalize records are capped at
     * kMaxPostFinalizeRecs and never count as overflow. Updates the record counters.
     *
     * @param n     Total record length in bytes (header + payload).
     * @param post  true for a post-finalize record.
     * @param tUs   Record time on the capture clock (ProbeClock µs).
     * @return Pointer to n reserved bytes in the buffer, or nullptr if dropped.
     */
    uint8_t* reserveRec(size_t n, bool post, int64_t tUs);

    /**
     * @brief Write the 7-byte record header: kind, len, flags (bit7 = post, bits0-6 = flag),
     *        int32 LE time relative to tBaseUs_.
     * @param p     Record start (from reserveRec()).
     * @param kind  Record kind.
     * @param len   Total record length in bytes.
     * @param post  Post-finalize record.
     * @param flag  Event flag (low 7 bits stored).
     * @param tUs   Record time on the capture clock (ProbeClock µs); range checked by reserveRec().
     */
    void writeHdr(uint8_t* p, uint8_t kind, uint8_t len, bool post, uint8_t flag,
                  int64_t tUs) const;

    /**
     * @brief Check that tUs fits the int32 µs offset from tBaseUs_ (about ±35 min).
     * @param tUs  Time on the capture clock (ProbeClock µs).
     * @return true if representable in a record header.
     */
    bool timeFits(int64_t tUs) const;

    /**
     * @brief Map a media region tag/kind pair to its MediaLayout::Region code.
     * @param tag   Region tag string.
     * @param kind  Region kind string.
     * @return Region index, or kRegionInline (0xFF) if null or no region matches.
     */
    static uint8_t regionCode(const char* tag, const char* kind);

    /**
     * @brief Check that a record payload has the exact length its kind requires.
     *
     * Every kind has a fixed or self-describing payload length, so a corrupt length byte
     * (which would shift all following records) is caught instead of replayed as garbage.
     *
     * @param kind     Record kind.
     * @param body     Payload (after the 7-byte header).
     * @param bodyLen  Payload length in bytes.
     * @return true if the payload shape is valid; false for unknown kinds.
     */
    static bool recShapeOk(uint8_t kind, const uint8_t* body, size_t bodyLen);

    /**
     * @brief Record and log a replay error at the current replay position.
     * @param why  Static error string ("count_mismatch", "truncated_header", "bad_length", "bad_shape").
     * @return Always false (lets replayOneRec() return it directly).
     */
    bool replayFail(const char* why);

    /** @brief True if every stored record was replayed, none malformed. */
    bool replayComplete() const;

    /**
     * @brief Set the experiment marker names cycled by BOOT short presses.
     * @param markers  Marker names (null entries skipped, max 15 chars each); nullptr or
     *                 count 0 = default (play_start, track_next, audible_change, replay, stop).
     * @param count    Number of entries (at most kMaxMarkers = 8 used).
     */
    void setMarkerSequence(const char* const* markers, uint8_t count);

    /** @brief Store the next name of the experiment sequence (cyclic) as marker at now; no-op unless capturing. */
    void queueNextExperimentMarker();

    /**
     * @brief Store the next free marker "mark_NN" (free-marker mode).
     * @param tUs  Time of the button press (ProbeClock µs).
     */
    void queueFreeMarker(int64_t tUs);

    /**
     * @brief Mark "soft_replug" and cycle D+ for kSoftReplugHoldMs (power stays; the HU sees detach → attach).
     *
     * Must not end the capture even if "unmount ends capture" is on: unplug/umount are
     * ignored for the hold time + 2.5 s.
     */
    void softReplug();

    /**
     * @brief Snapshot the finished run into lastRun_ and compute the probe-side integrity gate.
     *
     * Probe-side gate (collector still checks recv_seq_gaps): overflow/drops → FAIL.
     *
     * @param complete  Upload delivered and replay complete.
     * @param dropped   EventBus events dropped during the upload.
     */
    void saveLastRun(bool complete, uint32_t dropped);

    /**
     * @brief Write [persistedLen_, end) to the TripStore; true when caught up.
     *
     * When the journal is full, persistFull_ is set and the rest stays RAM only.
     *
     * @param end       Buffer offset to persist up to.
     * @param maxBytes  Maximum bytes written in this call (bounds one flash stall).
     * @return true if caught up or the journal is full; false if more remains.
     * @warning Writes flash (cache stall); call only in USB idle gaps or after the capture.
     */
    bool persistUpTo(size_t end, size_t maxBytes);

    /**
     * @brief Drive mode: advance an incremental checkpoint of the buffer to the TripStore.
     *
     * Starts every kCheckpointEveryMs when USB is idle (no CBW for kCheckpointIdleMs), or
     * forced after kCheckpointForceMs; writes at most kCheckpointChunk bytes per call. The
     * newest DATA record may still grow by coalescing and is left for the next checkpoint.
     * A finished checkpoint is committed and marked "ckpt:<ms>:<KiB>".
     */
    void checkpointStep();

    /** @brief Drive mode: persist the rest of the buffer and close the trip with the stop reason. */
    void finalPersist();

    /**
     * @brief Leave Uploading: free buffer, restore USB, mark the trip uploaded if delivered.
     *
     * Also clears drive mode (persist) and the stored-replay flag; state becomes Idle.
     *
     * @param delivered  All events reached the collector.
     */
    void finishRun(bool delivered);

    static constexpr size_t kDefaultCap = 64 * 1024;
    static constexpr uint32_t kMaxCaptureMs = 1800000;  // 30 min; int32 time base allows ±35 min
    static constexpr uint32_t kDoubleTapMs = 500;
    static constexpr uint16_t kSoftReplugHoldMs = 2000;
    static constexpr uint32_t kUnplugIdleMs = 1500;
    static constexpr uint32_t kUploadStallMs = 120000;
    /** @brief Stored trip: leave Replay/Drain if no TCP progress for this long. */
    static constexpr uint32_t kStoredUploadStallMs = 60000;
    /** @brief Stored trip: hard cap before accepting status=queued with leftover ring lines. */
    static constexpr uint32_t kStoredUploadHardMs = 180000;
    /** @brief Bounce collector socket if no tcp_tx_ok while WiFi is up. */
    static constexpr uint32_t kStoredTcpForceMs = 12000;
    static constexpr uint32_t kWifiReconnectEveryMs = 10000;
    /** @brief Cap ring depth during stored replay — high pending starved WiFi/HTTP. */
    static constexpr size_t kReplayMaxPending = 24;
    static constexpr uint32_t kCheckpointEveryMs = 60000;
    static constexpr uint32_t kCheckpointForceMs = 90000;  // no idle gap for this long
    static constexpr uint32_t kCheckpointIdleMs = 500;     // no CBW for this long
    static constexpr size_t kCheckpointChunk = 8192;       // per poll pass, bounds one stall
    static constexpr uint32_t kStoredTcpWaitMs = 20000;
    static constexpr uint8_t kKindMarker = 100;
    static constexpr uint8_t kKindUmount = 8;
    static constexpr uint8_t kKindData = 2;
    static constexpr uint8_t kMaxMarkers = 8;
    static constexpr size_t kMarkerNameLen = 16;

    /** Post-finalize records are diagnostic; cap so they never crowd the capture. */
    static constexpr uint32_t kMaxPostFinalizeRecs = 96;

    /**
     * Variable-length records ("compact/1"); replay rebuilds the full events.
     * Header: kind, total length, flags (bit7 = post-finalize, bits0-6 = flag),
     * int32 LE capture time relative to tBaseUs_. Payload by kind:
     *   1 CBW:  tag, xfer_len, flags, lun, cdb_len, n, CDB[n] (trailing zero bytes cut;
     *           signature is constant)
     *   2 DATA: bot_tag, lba, bytes, dur_us, gap_us, chunks(u16), region
     *           (region 0xFF: tag, kind, dir follow as length-prefixed strings)
     *   3 CSW:  tag, residue, status
     *   5 control: bmRequestType, bRequest, wValue, wIndex, wLength
     *   6/7/8: wValue · 9/10: epAddr · 100 marker: name bytes
     */
    static constexpr size_t kRecHdr = 7;
    static constexpr size_t kRecMax = 64;
    static constexpr size_t kTypicalRecBytes = 22;  // TUR pair 19+16, READ10 28+30+16
    static constexpr uint32_t kEstBytesPerS = 80;       // field runs 2026-10-20: 26..75 B/s
    static constexpr uint32_t kEstFineBytesPerS = 480;  // 16 DATA records/s × 30 B
    static constexpr uint8_t kRegionInline = 0xFF;
    static constexpr size_t kNoRec = SIZE_MAX;

    State state_ = State::Idle;
    Session* session_ = nullptr;
    MscGadget* msc_ = nullptr;
    ReadInstrument* inst_ = nullptr;
    TcpStreamer* tcp_ = nullptr;

    /**
     * @brief Pump TCP until the EventBus is empty and connected, or until the timeout.
     * @param maxMs  Maximum blocking time in ms.
     */
    void pumpTcp(uint32_t maxMs);

    /**
     * @brief Drain ring during replay so a 254+ Rec capture cannot drop before WiFi TX.
     *
     * Returns at once if fewer than keepBelow events are pending.
     *
     * @param maxMs      Maximum blocking time in ms.
     * @param keepBelow  Pump until fewer than this many events are pending (and connected) or none.
     */
    void pumpTcpWhilePending(uint32_t maxMs, size_t keepBelow);

    /**
     * @brief Call WiFi.reconnect() at most every kWifiReconnectEveryMs.
     *
     * Association + DHCP needs several seconds; reconnecting on every loop pass aborts
     * each attempt (the old Drain loop never reconnected → frozen purple until RST).
     */
    void reconnectWifiThrottled();
    unsigned long lastWifiReconnectMs_ = 0;
    uint32_t armFreeHeap_ = 0;
    uint32_t armMaxAlloc_ = 0;

    uint8_t* buf_ = nullptr;
    size_t cap_ = 0;
    size_t used_ = 0;
    size_t lastRecOff_ = kNoRec;
    int64_t tBaseUs_ = 0;
    uint32_t replaySeq_ = 0;
    const char* replayError_ = nullptr;
    uint32_t timeRangeDropped_ = 0;

    /** Same instant on both clocks: maps capture_t_us to UTC (0 = no NTP time). */
    int64_t anchorUnixUs_ = 0;
    int64_t anchorCaptureUs_ = 0;
    uint32_t recordCount_ = 0;
    uint32_t overflow_ = 0;
    uint32_t postFinalizeRecords_ = 0;
    uint32_t postFinalizeDropped_ = 0;

    char wallclock_[32] = {0};
    String vehicle_;
    String scenario_;
    String profileId_;
    String stopReason_;
    String endStrategy_ = "media_hide";
    String uploadStatus_ = "idle";

    unsigned long captureStartedMs_ = 0;
    unsigned long unplugSinceMs_ = 0;
    bool stopRequested_ = false;
    bool wasPlugged_ = false;
    bool softUsb_ = true;
    uint32_t autoDetachMs_ = 0;
    bool autoDetached_ = false;
    bool mediaHidden_ = false;
    bool fallbackDetached_ = false;

    /** @brief Step of the upload state machine driven by pollUpload(). */
    enum class UploadPhase : uint8_t { Idle = 0, Wifi, Session, Replay, Health, Drain, Done };
    UploadPhase uploadPhase_ = UploadPhase::Idle;
    size_t replayOff_ = 0;
    bool sessionStarted_ = false;
    bool healthPublished_ = false;
    unsigned long uploadPhaseSinceMs_ = 0;
    uint32_t transportEpochStart_ = 0;
    uint32_t transportEpochEnd_ = 0;

    /** Ignore umount/unplug auto-stop during initial soft re-enum. */
    unsigned long ignoreUnplugUntilMs_ = 0;

    unsigned long bootDownMs_ = 0;
    int64_t bootDownUs_ = 0;
    bool bootWasDown_ = false;
    bool endOnUnmount_ = true;
    bool freeMarkers_ = false;
    bool fineData_ = false;
    uint16_t freeMarkIdx_ = 0;
    uint16_t softReplugs_ = 0;
    unsigned long pendingTapMs_ = 0;  // first short press waiting for a possible second
    int64_t pendingTapUs_ = 0;

    bool persist_ = false;
    bool persistFull_ = false;
    bool replayingStored_ = false;
    char persistMedia_[16] = {0};
    char captureFw_[16] = {0};
    uint32_t tripId_ = 0;
    size_t persistedLen_ = 0;
    uint16_t checkpoints_ = 0;
    unsigned long lastCbwMs_ = 0;
    unsigned long lastCheckpointMs_ = 0;
    bool ckptRunning_ = false;
    int64_t ckptStartUs_ = 0;
    uint32_t ckptMs_ = 0;
    uint32_t ckptBytes_ = 0;
    bool (*wifiConnect_)(uint32_t timeoutMs) = nullptr;

    char markerSeq_[kMaxMarkers][kMarkerNameLen] = {};
    uint8_t markerCount_ = 0;
    uint8_t markerIndex_ = 0;
    char lastMarker_[kMarkerNameLen] = {0};

    /** @brief Summary of the last finished run for status JSON (filled by saveLastRun()). */
    struct LastRun {
        bool valid = false;
        char wallclock[32] = {0};
        char vehicle[24] = {0};
        char scenario[32] = {0};
        char stopReason[32] = {0};
        char uploadStatus[24] = {0};
        char endStrategy[16] = {0};
        uint32_t records = 0;
        uint32_t bytes = 0;
        uint32_t cap = 0;
        uint32_t overflow = 0;
        uint32_t dropped = 0;
        uint32_t measureMs = 0;
        bool complete = false;
        bool integrityPass = false;
    } lastRun_;
};
