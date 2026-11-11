#pragma once

/**
 * @file FieldPlan.h
 * @brief Drive-mode job queue read from a GitHub-hosted field/plan.json (read-only, no token).
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "core/FieldPlanCore.h"

/**
 * @brief Field plan: which capture each trip runs, edited at home, fetched over any WLAN.
 *
 * plan.json: {"schema":1,"plan":"id","vehicle":"bmw","enabled":true,"arm_at_home":true,
 *             "online_only":false,
 *             "jobs":[{"id":"mix","experiment":"drive_home","media":"drive_mix",
 *                      "fine":false,"measure_s":1800,"repeat":4,
 *                      "stall":{"regions":["body"],"pace":"realtime"}}, …]}
 * "stall" (optional) is passed to ReadStall::configure when the capture is armed and ends
 * with the capture; a config the brake rejects leaves the run uncounted.
 * One trip (= one boot with a host mount) consumes one run; job k runs `repeat` times before
 * job k+1. The plan body and the number of runs done are kept in NVS, so trips without WLAN
 * use the last fetched plan. A new plan id restarts at run 0; the same id keeps the counter.
 */
namespace FieldPlan {

/**
 * @brief Load the cached plan and run counter from NVS.
 * @param vehicle  Configured vehicleId; the plan only applies if its "vehicle" matches.
 */
void begin(const String& vehicle);

/**
 * @brief Fetch the plan over HTTPS and replace the cached one if it is valid.
 *
 * An unreachable or invalid remote plan keeps the cached plan.
 *
 * @param url       https:// URL of plan.json (empty = feature off, nothing fetched).
 * @param[out] info Optional; receives the result, e.g. "same id=x", "new id=x", "http_404",
 *                  "invalid bad_repeat", "unknown_experiment".
 * @warning Blocks for the HTTPS request (up to ~10 s).
 */
void refresh(const String& url, String* info);

/**
 * @brief True if a valid, enabled plan for this vehicle still has runs left.
 */
bool active();

/** @brief True if the active plan also arms a capture on the home WLAN (outbound trip). */
bool armAtHome();

/**
 * @brief True if the plan forbids any capture on this boot.
 *
 * Set when the cached plan (enabled, this vehicle) has "online_only" and no refresh() since
 * boot succeeded, so switching the phone hotspot off disables drive captures. This also
 * blocks the non-plan autoCapture fallback once the plan's runs are used up.
 */
bool armBlocked();

/**
 * @brief Job for the next trip.
 * @return The job, or nullptr if no plan is active.
 */
const huprobe::PlanJob* currentJob();

/**
 * @brief Session scenario for the next trip ("fp_<plan>_<job>_r<rep>").
 * @param out  Output buffer (at least 29 bytes).
 * @param n    Size of out.
 * @return false if no plan is active or out is too small.
 */
bool currentScenario(char* out, size_t n);

/**
 * @brief The host mounted during a planned trip: count the run and persist the counter.
 */
void advance();

/**
 * @brief Serialize plan id, state, progress, next job and last fetch result.
 * @param obj  Target JSON object.
 */
void toJson(JsonObject obj);

}  // namespace FieldPlan
