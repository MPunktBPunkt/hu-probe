#pragma once

#include <Arduino.h>
#include "session/Session.h"

/** LBA/Timing-Hooks — wird vom MSC-Callback gefüttert. */
class ReadInstrument {
public:
    void begin(Session* session);
    void onRead(uint32_t lba, uint32_t nBytes, uint32_t durUs, const char* tag, const char* kind);
    void onScsi(const char* op);
    float readRateKbS() const { return readRateKbS_; }
    float turHz() const { return turHz_; }
    uint32_t readCount() const { return readCount_; }
    uint32_t writeCount() const { return writeCount_; }
    void tick();

private:
    Session* session_ = nullptr;
    unsigned long lastReadMs_ = 0;
    unsigned long windowStartMs_ = 0;
    uint32_t windowBytes_ = 0;
    uint32_t turCountWindow_ = 0;
    float readRateKbS_ = 0;
    float turHz_ = 0;
    uint32_t readCount_ = 0;
    uint32_t writeCount_ = 0;
};
