#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "BuildFlags.h"

/** Ringpuffer für JSONL-Events (Collector + lokaler Trace). Thread-safe. */
class EventBus {
public:
    static EventBus& instance();
    void begin();
    /** Drop all pending lines (frees String heap) — use before Offline RAM alloc. */
    void clear();
    void publish(JsonDocument& doc);
    /** Copy head without removing (for TCP flush-before-pop). */
    bool peek(String& out);
    /** Remove head after successful TX (or intentional discard). */
    bool pop(String& out);
    size_t pending() const;
    uint32_t dropped() const { return dropped_; }
    uint32_t highwater() const { return highwater_; }
    uint32_t tcpDisconnects() const { return tcpDisconnects_; }
    void noteTcpDisconnect() { tcpDisconnects_++; }
    void noteTcpConnect() { /* cumulative disconnects only */ }
    uint32_t published() const { return published_; }
    uint32_t streamLost() const { return streamLost_; }
    void noteStreamLost(uint32_t n = 1) { streamLost_ += n; }
    uint32_t tcpTxOk() const { return tcpTxOk_; }
    uint32_t tcpTxFail() const { return tcpTxFail_; }
    void noteTcpTxOk();
    void noteTcpTxFail() { tcpTxFail_++; }
    /** millis() of last successful TCP event TX (0 = never). */
    unsigned long lastTcpTxOkMs() const { return lastTcpTxOkMs_; }
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
    uint32_t streamLost_ = 0;
    uint32_t tcpTxOk_ = 0;
    uint32_t tcpTxFail_ = 0;
    unsigned long lastTcpTxOkMs_ = 0;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};
