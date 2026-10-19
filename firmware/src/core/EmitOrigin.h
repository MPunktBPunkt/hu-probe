#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Emit-origin context for BOT/phase/marker events (0.2.31, B7).
 *
 * Offline replay runs after Session::start(); ProbeClock::publishUs() clamps every
 * older Capture-tUs to the session-start watermark. While a context is set, events
 * additionally carry the unclamped `capture_t_us` (+ `capture_seq` for buffered
 * records) and an `origin` tag so analysis never mixes replayed capture data with
 * live events that arrive after media_hide.
 *
 * Single-threaded: set/cleared only from the Arduino loop (OfflineCapture poll and
 * MscBotTap drain run there).
 */
namespace EmitOrigin {

struct Ctx {
    const char* origin = nullptr;  // offline | post_finalize | post_finalize_buffered
    bool rawCapture = false;       // emit capture_t_us
    bool hasSeq = false;           // emit capture_seq
    uint32_t seq = 0;
};

inline Ctx& ctx() {
    static Ctx c;
    return c;
}

inline void set(const char* origin, bool rawCapture, bool hasSeq = false, uint32_t seq = 0) {
    Ctx& c = ctx();
    c.origin = origin;
    c.rawCapture = rawCapture;
    c.hasSeq = hasSeq;
    c.seq = seq;
}

inline void clear() { set(nullptr, false, false, 0); }

/** Add origin / capture_t_us / capture_seq if a context is active. */
inline void apply(JsonDocument& doc, int64_t rawCaptureUs) {
    const Ctx& c = ctx();
    if (!c.origin) return;
    doc["origin"] = c.origin;
    if (c.rawCapture && rawCaptureUs > 0) doc["capture_t_us"] = (long long)rawCaptureUs;
    if (c.hasSeq) doc["capture_seq"] = c.seq;
}

}  // namespace EmitOrigin
