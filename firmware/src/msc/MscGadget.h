#pragma once

#include <Arduino.h>
#include "session/Session.h"
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
        uint32_t tUs = 0;   // low 32 of probe clock (gaps use full int64 delta before store)
        uint32_t lba = 0;
        uint32_t n = 0;
        uint32_t durUs = 0;  // full µs — no uint16 cap (lab: gap hit 65535)
        uint32_t gapUs = 0;
        char tag[8] = {0};
        char kind[8] = {0};
        uint8_t scsi = 0;  // 0=read, 1=write note
    };

    void presentMedia(const char* reason);
    void bumpSerial();
    void pushSample(const Sample& s);
    void drainSamples();
    void tagForLba(uint32_t lba, char* tag, size_t n, char* kind, size_t kn) const;

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
    uint32_t plugCount_ = 0;
    uint32_t unplugCount_ = 0;
    uint32_t readCount_ = 0;
    int64_t lastReadEndUs_ = 0;

    static constexpr size_t kSampleRing = 512;
    Sample ring_[kSampleRing];
    volatile size_t head_ = 0;
    volatile size_t tail_ = 0;
    volatile size_t count_ = 0;
    uint32_t sampleOverflow_ = 0;
};
