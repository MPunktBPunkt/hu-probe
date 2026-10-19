#pragma once

#include <Arduino.h>
#include "msc/ReadInstrument.h"
#include "session/Session.h"
#include "msc/MscGadget.h"
#include "net/TcpStreamer.h"

/**
 * RAM Offline-Capture (Messkonzept).
 * WiFi OFF during measure → compact Pending records in heap → WiFi ON → replay JSONL.
 * No flash writes in the measure window (avoids cache stalls).
 *
 * 0.2.25: experiment marker sequence (BOOT short), capture budget, last-run integrity.
 */
class OfflineCapture {
public:
    enum class State : uint8_t {
        Idle = 0,
        Arming,
        Capturing,
        Finalizing,
        Uploading
    };

    static OfflineCapture& instance();

    State state() const { return state_; }
    bool active() const {
        return state_ == State::Arming || state_ == State::Capturing ||
               state_ == State::Finalizing || state_ == State::Uploading;
    }
    bool capturing() const { return state_ == State::Capturing; }
    bool blocksWifiSupervise() const { return active(); }

    uint32_t recordCount() const { return recordCount_; }
    uint32_t postFinalizeRecords() const { return postFinalizeRecords_; }
    uint32_t overflow() const { return overflow_; }
    size_t bytesUsed() const { return used_; }
    size_t bytesCap() const { return cap_; }
    const char* wallclock() const { return wallclock_; }
    const char* stateName() const;

    /** Typical record size (records are variable length). Used for capture_budget. */
    static constexpr size_t recordSize() { return kTypicalRecBytes; }
    static constexpr size_t defaultCapBytes() { return kDefaultCap; }
    static constexpr size_t fallbackCapBytes() { return 48 * 1024; }
    static constexpr uint32_t maxCaptureMs() { return kMaxCaptureMs; }
    static size_t maxRecordsForCap(size_t capBytes) {
        return capBytes / kTypicalRecBytes;
    }

    /** UI/API: stop live session, grab wallclock, WiFi OFF, alloc buffer. */
    bool arm(Session* session, MscGadget* msc, ReadInstrument* inst, TcpStreamer* tcp);
    /**
     * After arm: enter Capturing. If softUsb, D+ re-enum after WiFi stop.
     * If autoDetachMs>0, end using endStrategy (media_hide default, soft_detach legacy).
     * markers/markerCount: BOOT-short cycles these names offline (experiment profile).
     */
    bool beginCapture(const String& vehicle, const String& scenario, const String& profileId,
                      bool softUsb = true, uint32_t autoDetachMs = 0,
                      const String& endStrategy = "media_hide",
                      const char* const* markers = nullptr, uint8_t markerCount = 0,
                      bool endOnUnmount = true, bool freeMarkers = false);
    /** End capture (button / unplug+idle / max duration / auto soft-detach). */
    void requestStop(const char* reason);
    /** App loop: auto-stop, BOOT marker/stop, upload when requested. */
    void poll(MscGadget* msc);

    /**
     * True while drain must route BOT events into RAM: the capture window plus the
     * post-finalize gap (media hidden, WiFi/session not yet up) so host commands after
     * media_hide (e.g. tags 123–130 in 075344) are not lost before replay starts.
     */
    bool acceptsPending() const {
        if (!buf_) return false;
        if (state_ == State::Capturing) return true;
        return (state_ == State::Finalizing || state_ == State::Uploading) && !sessionStarted_;
    }

    /** Called from MscBotTap::drain instead of ReadInstrument when acceptsPending(). */
    bool appendPending(uint8_t kind, int64_t tUs, const uint8_t* raw31, uint32_t botTag,
                       uint32_t lba, uint32_t nBytes, uint32_t durUs, uint32_t gapUs,
                       uint16_t wValue, uint16_t wIndex, uint16_t wLength,
                       uint8_t bmRequestType, uint8_t bRequest, uint8_t epAddr, uint8_t flag,
                       const char* tag, const char* kindStr, const char* dir);

    void queueMarker(const char* name);
    /** Protocol-derived marker (AutoMarker) at Capture-tUs; also in the post-finalize gap. */
    void appendAutoMarker(int64_t tUs, const char* name);
    /** Overflow of the last finished run (SequenceRunner splits the step). */
    uint32_t lastRunOverflow() const { return lastRun_.valid ? lastRun_.overflow : 0; }
    bool lastRunIntegrity() const { return lastRun_.valid && lastRun_.integrityPass; }

    void toStatusJson(JsonObject obj) const;
    /** Fill capture_budget object (theoretical 64 KiB target + actual if armed). */
    void toBudgetJson(JsonObject obj) const;

private:
    OfflineCapture() = default;
    bool allocBuffer();
    void freeBuffer();
    bool wifiOff();
    bool wifiOn();
    void beginFinalize(MscGadget* msc);
    void pollUpload();
    bool replayOneRec();
    void publishOfflineHealth(bool final);
    bool drainUpload(uint32_t maxMs);
    void restoreUsbAfterUpload();
    void appendMarkerRec(int64_t tUs, const char* name);
    /** Space for one record of n bytes, or nullptr (counts overflow / post-finalize drop). */
    uint8_t* reserveRec(size_t n, bool post, int64_t tUs);
    void writeHdr(uint8_t* p, uint8_t kind, uint8_t len, bool post, uint8_t flag,
                  int64_t tUs) const;
    bool timeFits(int64_t tUs) const;
    static uint8_t regionCode(const char* tag, const char* kind);
    static bool recShapeOk(uint8_t kind, const uint8_t* body, size_t bodyLen);
    bool replayFail(const char* why);
    /** Every stored record replayed, none malformed. */
    bool replayComplete() const;
    void setMarkerSequence(const char* const* markers, uint8_t count);
    void queueNextExperimentMarker();
    void queueFreeMarker(int64_t tUs);
    void softReplug();
    void saveLastRun(bool complete, uint32_t dropped);

    static constexpr size_t kDefaultCap = 64 * 1024;
    static constexpr uint32_t kMaxCaptureMs = 600000;   // 10 min; int32 time base allows ±35 min
    static constexpr uint32_t kDoubleTapMs = 500;
    static constexpr uint16_t kSoftReplugHoldMs = 2000;
    static constexpr uint32_t kUnplugIdleMs = 1500;
    static constexpr uint32_t kUploadStallMs = 120000;
    static constexpr uint32_t kWifiReconnectEveryMs = 10000;
    static constexpr size_t kReplayMaxPending = 48;
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
    static constexpr uint8_t kRegionInline = 0xFF;
    static constexpr size_t kNoRec = SIZE_MAX;

    State state_ = State::Idle;
    Session* session_ = nullptr;
    MscGadget* msc_ = nullptr;
    ReadInstrument* inst_ = nullptr;
    TcpStreamer* tcp_ = nullptr;
    void pumpTcp(uint32_t maxMs);
    void pumpTcpWhilePending(uint32_t maxMs, size_t keepBelow);
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
    uint16_t freeMarkIdx_ = 0;
    uint16_t softReplugs_ = 0;
    unsigned long pendingTapMs_ = 0;  // first short press waiting for a possible second
    int64_t pendingTapUs_ = 0;

    char markerSeq_[kMaxMarkers][kMarkerNameLen] = {};
    uint8_t markerCount_ = 0;
    uint8_t markerIndex_ = 0;
    char lastMarker_[kMarkerNameLen] = {0};

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
