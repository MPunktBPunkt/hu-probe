#pragma once

#include <Arduino.h>
#include "session/Session.h"
#include "media/ProbeMedia.h"
#include "msc/ReadInstrument.h"

/**
 * Minimal TinyUSB MSC gadget for first HU field tests.
 * Soft USB cycle = tud_disconnect/connect (D+ pull-up), not VBUS.
 */
class MscGadget {
public:
    void begin(Session* session, ReadInstrument* instrument);
    void loop();

    bool ready() const { return ready_; }
    bool plugged() const { return plugged_; }
    bool suspended() const { return suspended_; }
    bool mediaPresented() const { return mediaPresented_; }
    bool usbSoftAttached() const { return softAttached_; }
    uint32_t transportEpoch() const { return transportEpoch_; }

    const char* identitySerial() const { return serial_; }
    const char* volumeLabel() const { return label_; }
    void setIdentity(const char* serial, const char* label);

    /** Soft bus detach (+ optional auto-reattach). */
    bool softDetach(uint16_t holdMs = 0);
    bool softAttach();
    /** Soft bus cycle (detach → hold → attach). */
    bool softCycle(uint16_t holdMs = 600);
    /** SCSI media not-ready window then present again (≠ bus cycle). */
    void remountMedia(bool bumpIdentity = true);
    /** Hide/restore only the LUN while keeping the USB transport attached. */
    bool hideMedia();
    bool restoreMedia();
    /**
     * 0.2.31: switch ProbeMedia to a preset (presetId) or custom spec (specJson).
     * LUN hidden while rebuilding; usbCycle=true re-enumerates (host re-reads
     * capacity/FS), else NOT READY window then present.
     */
    bool switchMedia(const char* presetId, const String* specJson, bool usbCycle, String* err,
                     bool persist = true);

    /** How the host is told about a live content change. */
    enum class MediaNotify : uint8_t { None, UnitAttention, Remount, Cycle };
    static bool parseNotify(const char* s, MediaNotify& out);
    static const char* notifyName(MediaNotify n);
    /**
     * Live file change (ProbeMedia::Mutation) now or after delayMs, then notify.
     * Markers: media_mutate (+ ua_request / ua_sent). Allowed during offline capture.
     * A new request replaces a still pending one.
     */
    bool mutateMedia(const ProbeMedia::Mutation& m, MediaNotify notify, uint32_t delayMs,
                     String* err);
    /** Notify only (no content change): UNIT ATTENTION, NOT READY remount or bus cycle. */
    bool notifyMedia(MediaNotify n);
    bool mutationPending() const { return mutPending_; }
    uint32_t mutationDueInMs() const;

    /** Keep the current serial across unplug/re-enum (SEQ-CACHE: cache key = content). */
    void setSerialPinned(bool on) { pinnedSerial_ = on; }
    bool serialPinned() const { return pinnedSerial_; }

    void onUsbPlugged(bool on);
    void onUsbSuspend(bool on);
    int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
    int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);

    uint32_t plugCount() const { return plugCount_; }
    uint32_t unplugCount() const { return unplugCount_; }
    uint32_t readCount() const { return readCount_; }
    uint32_t sampleOverflow() const { return sampleOverflow_; }

private:
    struct Sample {
        int64_t tUs = 0;  // MSC-callback capture time (must survive drain)
        uint32_t lba = 0;
        uint32_t n = 0;
        uint32_t durUs = 0;  // full µs — no uint16 cap (lab: gap hit 65535)
        uint32_t gapUs = 0;
        uint32_t botTag = 0;  // host CBW tag (TinyUSB)
        char tag[8] = {0};
        char kind[8] = {0};
        uint8_t scsi = 0;  // 0=read, 1=write note
    };

    void presentMedia(const char* reason);
    void bumpSerial();
    /** Serial/label from the active media variant (usb_serial pins the serial). */
    void applyMediaIdentity();
    void pushSample(const Sample& s);
    void drainSamples();
    void tagForLba(uint32_t lba, char* tag, size_t n, char* kind, size_t kn) const;
    bool applyMutationNow(const ProbeMedia::Mutation& m, MediaNotify notify, String* err);
    void mark(const char* name, const char* note, int64_t tUs = 0);
    void pollUnitAttention();

    bool mutPending_ = false;
    ProbeMedia::Mutation pendingMut_;
    MediaNotify pendingNotify_ = MediaNotify::None;
    uint32_t mutDueMs_ = 0;
    uint32_t uaSeen_ = 0;

    Session* session_ = nullptr;
    ReadInstrument* instrument_ = nullptr;
    bool ready_ = false;
    bool plugged_ = false;
    bool suspended_ = false;
    bool mediaPresented_ = false;
    bool softAttached_ = true;
    uint32_t remountHoldUntilMs_ = 0;
    uint32_t softReattachAtMs_ = 0;
    uint16_t remountGen_ = 1;
    char serial_[16] = "HP0001";
    char label_[12] = "HUPROBE";
    bool fixedSerial_ = false;
    bool pinnedSerial_ = false;
    uint32_t plugCount_ = 0;
    uint32_t unplugCount_ = 0;
    uint32_t readCount_ = 0;
    uint32_t transportEpoch_ = 0;
    int64_t lastReadEndUs_ = 0;

    // Only write notes use this ring since 0.2.12 (reads go through MscBotTap);
    // 32 instead of 512 frees ~25 KiB DRAM for the offline buffer and SynthFat.
    static constexpr size_t kSampleRing = 32;
    Sample ring_[kSampleRing];
    volatile size_t head_ = 0;
    volatile size_t tail_ = 0;
    volatile size_t count_ = 0;
    uint32_t sampleOverflow_ = 0;
};
