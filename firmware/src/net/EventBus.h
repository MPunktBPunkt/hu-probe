#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "BuildFlags.h"

/** Ringpuffer für JSONL-Events (Collector + lokaler Trace). */
class EventBus {
public:
    static EventBus& instance();
    void begin();
    void publish(JsonDocument& doc);
    bool pop(String& out);
    size_t pending() const;
    uint32_t dropped() const { return dropped_; }
    uint32_t highwater() const { return highwater_; }
    uint32_t tcpDisconnects() const { return tcpDisconnects_; }
    void noteTcpDisconnect() { tcpDisconnects_++; }
    void noteTcpConnect() { /* counter stays cumulative */ }
    uint32_t published() const { return published_; }
    void fillCaptureHealth(JsonObject obj, uint32_t seqFirst, uint32_t seqLast,
                           bool tcpConnected) const;

private:
    EventBus() = default;
    String ring_[EVENT_RING_SIZE];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    uint32_t dropped_ = 0;
    uint32_t highwater_ = 0;
    uint32_t tcpDisconnects_ = 0;
    uint32_t published_ = 0;
};
