#pragma once

#include <Arduino.h>
#include "msc/ReadInstrument.h"
#include "session/Session.h"
#include "msc/MscGadget.h"

/**
 * 0.2.19 Stufe-1: RAM Offline-Capture (Messkonzept Offline-Capture).
 * WiFi OFF during measure → compact Pending records in heap → WiFi ON → replay JSONL.
 * No flash writes in the measure window (avoids cache stalls).
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
    uint32_t overflow() const { return overflow_; }
    size_t bytesUsed() const { return used_; }
    size_t bytesCap() const { return cap_; }
    const char* wallclock() const { return wallclock_; }
    const char* stateName() const;

    /** UI/API: stop live session, grab wallclock, WiFi OFF, alloc buffer. */
    bool arm(Session* session, MscGadget* msc, ReadInstrument* inst);
    /**
     * After arm: enter Capturing. If softUsb, D+ re-enum after WiFi stop.
     * If autoDetachMs>0, soft-detach after that many ms (SW unplug — no HTTP needed).
     */
    bool beginCapture(const String& vehicle, const String& scenario, const String& profileId,
                      bool softUsb = true, uint32_t autoDetachMs = 0);
    /** End capture (button / unplug+idle / max duration / auto soft-detach). */
    void requestStop(const char* reason);
    /** App loop: auto-stop, BOOT marker/stop, upload when requested. */
    void poll(MscGadget* msc);

    /** Called from MscBotTap::drain instead of ReadInstrument when capturing. */
    bool appendPending(uint8_t kind, int64_t tUs, const uint8_t* raw31, uint32_t botTag,
                       uint32_t lba, uint32_t nBytes, uint32_t durUs, uint32_t gapUs,
                       uint16_t wValue, uint16_t wIndex, uint16_t wLength,
                       uint8_t bmRequestType, uint8_t bRequest, uint8_t epAddr, uint8_t flag,
                       const char* tag, const char* kindStr, const char* dir);

    void queueMarker(const char* name);

    void toStatusJson(JsonObject obj) const;

private:
    OfflineCapture() = default;
    bool allocBuffer();
    void freeBuffer();
    bool wifiOff();
    bool wifiOn();
    bool finalizeAndUpload();
    bool replayAll();
    void appendMarkerRec(int64_t tUs, const char* name);

    static constexpr size_t kDefaultCap = 64 * 1024;
    static constexpr uint32_t kMaxCaptureMs = 180000;   // 3 min safety
    static constexpr uint32_t kUnplugIdleMs = 1500;
    static constexpr uint8_t kKindMarker = 100;
    static constexpr uint8_t kKindUmount = 8;

#pragma pack(push, 1)
    struct Rec {
        uint8_t kind;
        uint8_t flag;
        uint8_t bmRequestType;
        uint8_t bRequest;
        uint8_t epAddr;
        uint8_t pad;
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
        int64_t tUs;
        uint32_t botTag;
        uint32_t lba;
        uint32_t nBytes;
        uint32_t durUs;
        uint32_t gapUs;
        uint8_t raw[31];
        char tag[8];
        char kindStr[8];
        char dir[4];
    };
#pragma pack(pop)

    State state_ = State::Idle;
    Session* session_ = nullptr;
    MscGadget* msc_ = nullptr;
    ReadInstrument* inst_ = nullptr;

    uint8_t* buf_ = nullptr;
    size_t cap_ = 0;
    size_t used_ = 0;
    uint32_t recordCount_ = 0;
    uint32_t overflow_ = 0;

    char wallclock_[32] = {0};
    String vehicle_;
    String scenario_;
    String profileId_;
    String stopReason_;

    unsigned long captureStartedMs_ = 0;
    unsigned long unplugSinceMs_ = 0;
    bool stopRequested_ = false;
    bool wasPlugged_ = false;
    bool softUsb_ = true;
    uint32_t autoDetachMs_ = 0;
    bool autoDetached_ = false;
    /** Ignore umount/unplug auto-stop during initial soft re-enum. */
    unsigned long ignoreUnplugUntilMs_ = 0;

    unsigned long bootDownMs_ = 0;
    bool bootWasDown_ = false;
};
