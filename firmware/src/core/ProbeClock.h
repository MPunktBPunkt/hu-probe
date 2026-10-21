#pragma once

/**
 * @file ProbeClock.h
 * @brief Monotonic 64-bit microsecond clock with an event-timestamp watermark.
 */

#include <Arduino.h>
#include <esp_timer.h>

/**
 * @brief Monotonic probe clock in microseconds.
 *
 * Uses esp_timer_get_time() (int64, does not wrap at 71 min like micros()).
 *
 * eventNowUs() never goes below the last Capture-tUs observed via observeEventTUs()
 * — keeps status/health from regressing past deferred BOT stamps (0.2.13).
 */
namespace ProbeClock {
/**
 * @brief Return the watermark storage (highest event timestamp seen, µs).
 * @return Reference to the static watermark, initially 0.
 */
inline int64_t& lastEventTUsRef() {
    static int64_t last = 0;
    return last;
}

/** @brief Microseconds since boot from esp_timer_get_time() (does not wrap). */
inline int64_t nowUs() {
    return esp_timer_get_time();
}

/**
 * @brief Raise the watermark to tUs if it is newer.
 * @param tUs  Event timestamp in µs since boot.
 */
inline void observeEventTUs(int64_t tUs) {
    if (tUs > lastEventTUsRef()) lastEventTUsRef() = tUs;
}

/**
 * @brief Clamp Capture-tUs up to last published, then raise watermark. Use for every emit.
 * @param tUs  Capture timestamp in µs since boot.
 * @return max(tUs, watermark) in µs; published timestamps are thus non-decreasing.
 */
inline int64_t publishUs(int64_t tUs) {
    int64_t last = lastEventTUsRef();
    if (tUs < last) tUs = last;
    observeEventTUs(tUs);
    return tUs;
}

/** @brief Current time in µs, but never below the event watermark (does not modify it). */
inline int64_t eventNowUs() {
    int64_t n = nowUs();
    int64_t last = lastEventTUsRef();
    return (n < last) ? last : n;
}

/**
 * @brief Convert a µs timestamp to a 32-bit millisecond value for legacy fields.
 * @param tUs  Timestamp in µs.
 * @return tUs / 1000 truncated to uint32_t (wraps after ~49.7 days); 0 for negative input.
 */
inline uint32_t nowMsCompat(int64_t tUs) {
    if (tUs < 0) return 0;
    return (uint32_t)(tUs / 1000LL);
}
}
