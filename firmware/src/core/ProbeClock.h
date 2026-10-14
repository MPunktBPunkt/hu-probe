#pragma once

#include <Arduino.h>
#include <esp_timer.h>

/**
 * Monotonic probe clock in microseconds.
 * Uses esp_timer_get_time() (int64, does not wrap at 71 min like micros()).
 *
 * eventNowUs() never goes below the last Capture-tUs observed via observeEventTUs()
 * — keeps status/health from regressing past deferred BOT stamps (0.2.13).
 */
namespace ProbeClock {
inline int64_t& lastEventTUsRef() {
    static int64_t last = 0;
    return last;
}
inline int64_t nowUs() {
    return esp_timer_get_time();
}
inline void observeEventTUs(int64_t tUs) {
    if (tUs > lastEventTUsRef()) lastEventTUsRef() = tUs;
}
/** Clamp Capture-tUs up to last published, then raise watermark. Use for every emit. */
inline int64_t publishUs(int64_t tUs) {
    int64_t last = lastEventTUsRef();
    if (tUs < last) tUs = last;
    observeEventTUs(tUs);
    return tUs;
}
inline int64_t eventNowUs() {
    int64_t n = nowUs();
    int64_t last = lastEventTUsRef();
    return (n < last) ? last : n;
}
inline uint32_t nowMsCompat(int64_t tUs) {
    if (tUs < 0) return 0;
    return (uint32_t)(tUs / 1000LL);
}
}
