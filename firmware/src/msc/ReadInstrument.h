#pragma once

#include <Arduino.h>
#include "session/Session.h"

/** LBA/Timing-/BOT-Hooks — wird vom MSC-Callback / Sample-Ring gefüttert. */
class ReadInstrument {
public:
    void begin(Session* session);
    /** Komfort-Pfad (Sim / Aggregat). Für echte Evidenz: onBot* / emitBotRead10Synth. */
    void onRead(uint32_t lba, uint32_t nBytes, uint32_t durUs, const char* tag, const char* kind);
    /**
     * Stufe A fallback: synthetische BOT-Kette. Ab 0.2.8 Drain nutzt TinyUSB-Tap (Stufe B).
     */
    void emitBotRead10Synth(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs,
                            uint32_t gapUs, const char* tag, const char* kind);
    /** Deprecated Kurzform — nur Stub/Sim. */
    void onScsi(const char* op);
    void onUsb(const char* usbEv, uint8_t ep, const char* dir, uint32_t durUs,
               const char* note = nullptr);
    /** Known gadget identity as usb get_descriptor timeline (no host control sniff yet). */
    void emitIdentityDescriptors(const char* serial, const char* vendor, const char* product,
                                 const char* revision);
    void onBotCbw(uint32_t botTag, const char* cdbHex, const char* opcode,
                  uint32_t lba, uint16_t blocks, uint32_t xferLen, uint8_t flags);
    void onBotData(uint32_t botTag, const char* dir, uint32_t expected, uint32_t actual,
                   uint32_t durUs, uint32_t lba, const char* tag, const char* kind);
    void onBotCsw(uint32_t botTag, uint32_t residue, uint8_t status);

    /** GATE-01B: host CBW/CSW bytes from TinyUSB `_mscd_itf` (`synthesized=false`). */
    void emitBotCbwTinyusb(const uint8_t cbw[31], int64_t tUs);
    /** bot_data + comfort read at same Capture-tUs (keeps seq/t_us non-decreasing). */
    void emitBotDataTinyusb(int64_t tUs, uint32_t botTag, uint32_t lba, uint32_t nBytes,
                            uint32_t durUs, uint32_t gapUs, const char* tag, const char* kind,
                            const char* dir);
    void emitBotCswTinyusb(const uint8_t csw[13], int64_t tUs);
    void emitUsbControlTinyusb(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue,
                               uint16_t wIndex, uint16_t wLength, int64_t tUs);
    /** TinyUSB MSC_STAGE_NEED_RESET (invalid CBW → stall both EPs). */
    void emitBotNeedResetTinyusb(int64_t tUs);
    /** Comfort `read` only (BOT chain already emitted as tinyusb). */
    void emitReadAggregate(int64_t tUs, uint32_t botTag, uint32_t lba, uint32_t nBytes,
                           uint32_t durUs, uint32_t gapUs, const char* tag, const char* kind);

    float readRateKbS() const { return readRateKbS_; }
    float turHz() const { return turHz_; }
    uint32_t readCount() const { return readCount_; }
    uint32_t writeCount() const { return writeCount_; }
    uint32_t botTag() const { return botTag_; }
    void tick();

private:
    void stamp(JsonDocument& doc);
    void stampAt(JsonDocument& doc, int64_t tUs);
    void bytesToHex(const uint8_t* in, size_t n, char* out, size_t outLen);

    Session* session_ = nullptr;
    int64_t lastDataEndUs_ = 0;
    unsigned long windowStartMs_ = 0;
    uint32_t windowBytes_ = 0;
    uint32_t turCountWindow_ = 0;
    float readRateKbS_ = 0;
    float turHz_ = 0;
    uint32_t readCount_ = 0;
    uint32_t writeCount_ = 0;
    uint32_t botTag_ = 0;
};
