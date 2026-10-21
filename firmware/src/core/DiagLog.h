#pragma once

/**
 * @file DiagLog.h
 * @brief Diagnostic text log (Serial + PSRAM ring + reset-surviving RTC ring).
 */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Diagnostic text log for the WiFi/offline/USB lifecycle without a UART cable.
 *
 * Every line goes to Serial, to a PSRAM ring (64 KiB, readable via /api/diag/log) and to
 * a 2 KiB RTC ring that survives software reset, panic and watchdog. The RTC part of the
 * previous boot is sent once per session as `diag_tail` in capture_health.
 *
 * Critical snapshots (boot after panic/WDT, planned restart, coex-hold, capture end,
 * drive-mode checkpoints) are also written to the first 32 KiB of the unused `spiffs`
 * partition so they survive power loss. USB bulk paths never call persist() — flash
 * erase would stall the host. Safe to call printf from the loop and from wifiCoexTask
 * (spinlock around the ring copy).
 */
namespace DiagLog {

/**
 * @brief Rescue the previous boot's RTC tail, reset the RTC ring and allocate the PSRAM ring.
 *
 * The rescued tail starts at a line boundary (a possibly cut first line is skipped when the
 * RTC ring had wrapped). The 64 KiB ring is only used if it lands in PSRAM; otherwise it is
 * freed again and only the RTC ring is written.
 * @note Call once at boot before the first printf().
 */
void begin();

/**
 * @brief printf to Serial plus both rings; prefixed with millis().
 *
 * The millis() prefix is only stored in the rings, Serial gets the bare message. A line is
 * limited to 192 bytes (truncated) and always terminated with '\n'.
 *
 * @param fmt  printf-style format string.
 * @param ...  Format arguments.
 */
void printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Return the RTC tail kept from the previous boot (empty after power-on).
 * @return NUL-terminated text (up to 2 KiB), valid for the lifetime of the program.
 */
const char* previousTail();

/**
 * @brief Adds `diag_tail` once per session id (no-op if empty or already sent for sid).
 * @param obj  JSON object (capture_health) to receive the "diag_tail" key.
 * @param sid  Session id; null or empty is a no-op.
 */
void appendPreviousTail(JsonObject obj, const char* sid);

/**
 * @brief Return the number of bytes currently in the PSRAM ring.
 * @return Byte count, 0..65536 (0 if the ring was not allocated).
 */
size_t size();

/**
 * @brief Copy n bytes starting at offset (oldest = 0) of the PSRAM ring.
 * @param offset  Start offset relative to the oldest byte in the ring.
 * @param[out] dst  Destination buffer of at least n bytes (not NUL-terminated).
 * @param n       Maximum number of bytes to copy.
 * @return Bytes copied; 0 if offset is past the end, the ring is missing or dst is null.
 */
size_t copyOut(size_t offset, char* dst, size_t n);

/**
 * @brief Write a flash snapshot of the recent log plus breadcrumbs.
 *
 * Builds a 4 KiB slot (header + last ~3.5 KiB of the PSRAM ring, previous RTC tail on
 * boot, current PanicTail crumbs) in the `spiffs` prefix ring (8 slots). Rate-limited
 * to 10 s (60 s when @p reason is "ckpt") unless @p force is true. No-op without the
 * partition, while another persist() is running, or when the snapshot buffer is missing.
 *
 * @param reason  Short tag stored in the slot ("boot", "restart", "coex", "drive",
 *                "ckpt", "capture", "webui"); may be nullptr ("?" ).
 * @param force   Ignore the rate limit (planned restart, first boot dump).
 * @return true if a slot was erased and written.
 * @warning Erases 4 KiB of flash; call only with USB idle, detached, or after capture.
 */
bool persist(const char* reason, bool force = false);

/** @brief True once begin() found the `spiffs` partition used for snapshots. */
bool persistAvailable();

/** @brief Number of valid flash snapshots (0..8). */
uint8_t snapshotCount();

/**
 * @brief Copy a slice of one snapshot (0 = newest) as plain text including its header line.
 * @param newestIndex  0 = most recently written slot.
 * @param offset       Byte offset in the virtual text (header line + body).
 * @param[out] dst     Destination buffer (not necessarily NUL-terminated).
 * @param n            Maximum bytes to copy.
 * @return Bytes copied; 0 if the index is out of range, offset is past the end, or dst is null.
 */
size_t copySnapshot(uint8_t newestIndex, size_t offset, char* dst, size_t n);

/**
 * @brief Fill a JSON object with ring/snapshot counters for /api/status.
 * @param[out] obj  Target object (keys: ring_bytes, prev_tail_bytes, saved, persist_ok).
 */
void appendStatus(JsonObject obj);

}  // namespace DiagLog
