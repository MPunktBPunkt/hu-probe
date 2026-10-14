#pragma once

#include <Arduino.h>
#include "session/Session.h"

/** LBA/Timing-/BOT-Hooks — wird vom MSC-Callback gefüttert. */
class ReadInstrument {
public:
    void begin(Session* session);
    /** Komfort-Pfad (Sim / Aggregat). Für echte Evidenz: onBot*. */
    void onRead(uint32_t lba, uint32_t nBytes, uint32_t durUs, const char* tag, const char* kind);
    /** Aus MSC-Sample-Ring: tUs/gap bereits zur Capture-Zeit gemessen. */
    void onReadTimed(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs, uint32_t gapUs,
                     const char* tag, const char* kind);
    /** Deprecated Kurzform — nur Stub/Sim. */
    void onScsi(const char* op);
    void onUsb(const char* usbEv, uint8_t ep, const char* dir, uint32_t durUs,
               const char* note = nullptr);
    void onBotCbw(uint32_t botTag, const char* cdbHex, const char* opcode,
                  uint32_t lba, uint16_t blocks, uint32_t xferLen, uint8_t flags);
    void onBotData(uint32_t botTag, const char* dir, uint32_t expected, uint32_t actual,
                   uint32_t durUs, uint32_t lba, const char* tag, const char* kind);
    void onBotCsw(uint32_t botTag, uint32_t residue, uint8_t status);

    float readRateKbS() const { return readRateKbS_; }
    float turHz() const { return turHz_; }
    uint32_t readCount() const { return readCount_; }
    uint32_t writeCount() const { return writeCount_; }
    void tick();

private:
    void stamp(JsonDocument& doc);
    void stampAt(JsonDocument& doc, int64_t tUs);

    Session* session_ = nullptr;
    int64_t lastDataEndUs_ = 0;
    unsigned long windowStartMs_ = 0;
    uint32_t windowBytes_ = 0;
    uint32_t turCountWindow_ = 0;
    float readRateKbS_ = 0;
    float turHz_ = 0;
    uint32_t readCount_ = 0;
    uint32_t writeCount_ = 0;
};
