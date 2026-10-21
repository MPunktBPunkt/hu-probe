#pragma once

/**
 * @file PhaseGate.h
 * @brief RTC + NVS ENTER/PASS checkpoints to localize hangs and crashes across reset.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Nested ENTER/PASS gates (RTC + NVS mirror).
 *
 * Call enter() immediately before a risky step and pass() only after it completed.
 * If the device resets with an open ENTER (no matching PASS), begin() latches that
 * phase as last_crash for /api/status and DiagLog.
 *
 * RTC covers soft-reset/WDT/panic (validated with CRC). NVS mirrors the open top so
 * power_on RST still reports last_crash. DiagLog snapshots also embed the open stack.
 */
namespace PhaseGate {

/** @brief Coarse/fine phases for trip upload (extend later for coex/USB). */
enum Phase : uint8_t {
    None = 0,
    TripBatch = 1,          ///< Whole pending-trips upload job
    TripReplay = 2,         ///< OfflineCapture::replayStored for one trip (arg=trip_id)
    TripDrain = 3,          ///< WebUi post-replay EventBus drain
    TripTcpForce = 4,       ///< TcpStreamer::forceReconnect during drain
    TripWifiRe = 5,         ///< WiFi.reconnect during drain
    StoredOcDrain = 6,      ///< OfflineCapture UploadPhase::Drain (stored)
    StoredQueuedClear = 7,  ///< Ring clear after stored hard-timeout
};

/**
 * @brief Capture incomplete ENTER from the previous boot, then start a fresh stack.
 * @note Call once early in App::begin(), after PanicTail::begin() / DiagLog::begin().
 */
void begin();

/**
 * @brief Push a phase as open (ENTER). arg is phase-specific (e.g. trip id).
 * @param phase  Phase code; None is ignored.
 * @param arg    Optional argument stored with the ENTER.
 */
void enter(Phase phase, uint32_t arg = 0);

/**
 * @brief Mark PASS for phase if it is the current top of the stack (pop).
 * @param phase  Must match the open top; mismatch is logged and ignored.
 */
void pass(Phase phase);

/**
 * @brief End a parent phase: auto-close any nested open frames, then PASS @p phase.
 * @param phase  Parent phase to close (typically TripBatch).
 */
void finish(Phase phase);

/** @brief Snake_case name for a phase code. */
const char* name(Phase phase);

/**
 * @brief Format current open stack as one text line for DiagLog snapshots.
 * @param[out] dst  Destination buffer (NUL-terminated when n > 0).
 * @param n         Capacity.
 * @return Bytes written (excluding NUL).
 */
size_t formatOpenLine(char* dst, size_t n);

/**
 * @brief Add phase_gate object: open stack, last_crash (if any), last_pass.
 * @param obj  Target JSON object (usually the /api/status root).
 */
void appendStatus(JsonObject obj);

/** @brief Previous-boot crash phase (None if clean). */
Phase lastCrashPhase();

/** @brief Argument of lastCrashPhase(). */
uint32_t lastCrashArg();

}  // namespace PhaseGate
