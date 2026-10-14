#pragma once

#include <Arduino.h>
#include <esp_timer.h>

/**
 * Monotonic probe clock in microseconds.
 * Uses esp_timer_get_time() (int64, does not wrap at 71 min like micros()).
 */
namespace ProbeClock {
inline int64_t nowUs() {
    return esp_timer_get_time();
}
inline uint32_t nowMsCompat(int64_t tUs) {
    if (tUs < 0) return 0;
    return (uint32_t)(tUs / 1000LL);
}
}
