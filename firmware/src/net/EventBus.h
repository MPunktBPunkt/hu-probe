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

private:
    EventBus() = default;
    String ring_[EVENT_RING_SIZE];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    uint32_t dropped_ = 0;
};
