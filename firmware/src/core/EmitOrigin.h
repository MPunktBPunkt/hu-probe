#pragma once

/**
 * @file EmitOrigin.h
 * @brief Origin tagging (offline / post_finalize) for replayed capture events.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Emit-origin context for BOT/phase/marker events (0.2.31, B7).
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

/** @brief Active emit-origin context; origin == nullptr means no context (live events). */
struct Ctx {
    const char* origin = nullptr;  // offline | post_finalize | post_finalize_buffered
    bool rawCapture = false;       // emit capture_t_us
    bool hasSeq = false;           // emit capture_seq
    uint32_t seq = 0;
};

/**
 * @brief Return the process-wide context singleton.
 * @return Reference to the single static Ctx instance.
 */
inline Ctx& ctx() {
    static Ctx c;
    return c;
}

/**
 * @brief Activate an emit-origin context for subsequent events.
 * @param origin      Origin tag ("offline", "post_finalize", "post_finalize_buffered");
 *                    must be a string with static lifetime; nullptr deactivates.
 * @param rawCapture  true to emit the unclamped `capture_t_us`.
 * @param hasSeq      true to emit `capture_seq` (buffered records).
 * @param seq         Capture sequence number emitted as `capture_seq` when hasSeq is true.
 */
inline void set(const char* origin, bool rawCapture, bool hasSeq = false, uint32_t seq = 0) {
    Ctx& c = ctx();
    c.origin = origin;
    c.rawCapture = rawCapture;
    c.hasSeq = hasSeq;
    c.seq = seq;
}

/** @brief Deactivate the context so subsequent events are emitted as live (no origin tag). */
inline void clear() { set(nullptr, false, false, 0); }

/**
 * @brief Add origin / capture_t_us / capture_seq if a context is active.
 * @param doc           Event document to extend; unchanged if no context is active.
 * @param rawCaptureUs  Unclamped capture timestamp in µs; `capture_t_us` is only written
 *                      if rawCapture is set and the value is > 0.
 */
inline void apply(JsonDocument& doc, int64_t rawCaptureUs) {
    const Ctx& c = ctx();
    if (!c.origin) return;
    doc["origin"] = c.origin;
    if (c.rawCapture && rawCaptureUs > 0) doc["capture_t_us"] = (long long)rawCaptureUs;
    if (c.hasSeq) doc["capture_seq"] = c.seq;
}

}  // namespace EmitOrigin
