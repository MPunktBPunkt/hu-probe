#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Diagnostic text log for the WiFi/offline/USB lifecycle without a UART cable.
 *
 * Every line goes to Serial, to a PSRAM ring (64 KiB, readable via /api/diag/log) and to
 * a 2 KiB RTC ring that survives software reset, panic and watchdog. The RTC part of the
 * previous boot is sent once per session as `diag_tail` in capture_health.
 * Safe to call from the loop and from wifiCoexTask (spinlock around the ring copy).
 */
namespace DiagLog {

void begin();
/** printf to Serial plus both rings; prefixed with millis(). */
void printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
/** RTC tail kept from the previous boot (empty after power-on). */
const char* previousTail();
/** Adds `diag_tail` once per session id (no-op if empty or already sent for sid). */
void appendPreviousTail(JsonObject obj, const char* sid);
/** Bytes currently in the PSRAM ring. */
size_t size();
/** Copy n bytes starting at offset (oldest = 0) of the PSRAM ring; returns bytes copied. */
size_t copyOut(size_t offset, char* dst, size_t n);

}  // namespace DiagLog
