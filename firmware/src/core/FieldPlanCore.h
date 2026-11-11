#pragma once

/**
 * @file FieldPlanCore.h
 * @brief Host-testable field plan model: validation, run-to-job mapping, scenario names.
 *
 * Fetching over HTTPS, NVS persistence and the experiment/media lookup live in FieldPlan.cpp;
 * this header has no Arduino dependencies.
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace huprobe {

/** @brief Maximum number of jobs in one plan. */
constexpr uint8_t kPlanMaxJobs = 8;

/** @brief Buffer size of a plan id (12 characters + NUL). */
constexpr size_t kPlanIdLen = 13;

/** @brief Buffer size of a job id (8 characters + NUL). */
constexpr size_t kPlanJobIdLen = 9;

/** @brief Buffer size of vehicle, experiment and media ids (23 characters + NUL). */
constexpr size_t kPlanNameLen = 24;

/** @brief Buffer size of a job's compact READ10-brake JSON (127 characters + NUL). */
constexpr size_t kPlanStallLen = 128;

/** @brief Highest allowed repeat count of one job. */
constexpr uint8_t kPlanMaxRepeat = 50;

/** @brief Highest allowed measurement time in seconds (OfflineCapture limit). */
constexpr uint32_t kPlanMaxMeasureS = 1800;

/** @brief One job: the capture settings used for `repeat` consecutive trips. */
struct PlanJob {
    char id[kPlanJobIdLen];
    char experiment[kPlanNameLen];
    char media[kPlanNameLen];  // empty = keep the configured autoMedia
    bool fine;
    uint32_t measureS;         // 0 = experiment default
    uint8_t repeat;
    char stall[kPlanStallLen];  // ReadStall::configure JSON, empty = no brake
};

/** @brief A field plan as published in field/plan.json. */
struct Plan {
    char id[kPlanIdLen];
    char vehicle[kPlanNameLen];
    bool enabled;
    bool armAtHome;
    bool onlineOnly;  // arm only on boots that fetched the plan (hotspot on = measure)
    uint8_t jobCount;
    PlanJob jobs[kPlanMaxJobs];
};

/**
 * @brief Check an id token: 1..cap-1 characters out of [A-Za-z0-9_-].
 * @param s    Token (may be null).
 * @param cap  Buffer size the token must fit into, including NUL.
 * @return true if the token is valid.
 */
inline bool planTokenValid(const char* s, size_t cap) {
    if (!s || !s[0]) return false;
    size_t n = 0;
    for (; s[n]; n++) {
        const char c = s[n];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok || n + 1 >= cap) return false;
    }
    return true;
}

/**
 * @brief Number of trips the plan asks for (sum of all repeat counts).
 * @param p  Plan.
 * @return Total runs.
 */
inline uint16_t planTotalRuns(const Plan& p) {
    uint16_t total = 0;
    for (uint8_t i = 0; i < p.jobCount && i < kPlanMaxJobs; i++) total += p.jobs[i].repeat;
    return total;
}

/**
 * @brief Map a zero-based run index to its job.
 * @param p         Plan.
 * @param run       Zero-based run index (number of trips already done).
 * @param[out] rep  Optional; receives the one-based repetition within the job.
 * @return The job, or nullptr if run is beyond the plan.
 */
inline const PlanJob* planJobForRun(const Plan& p, uint16_t run, uint8_t* rep) {
    for (uint8_t i = 0; i < p.jobCount && i < kPlanMaxJobs; i++) {
        if (run < p.jobs[i].repeat) {
            if (rep) *rep = static_cast<uint8_t>(run + 1);
            return &p.jobs[i];
        }
        run = static_cast<uint16_t>(run - p.jobs[i].repeat);
    }
    return nullptr;
}

/**
 * @brief Structural validation (ids, counts, limits); experiment/media existence is not checked.
 * @param p  Plan.
 * @return nullptr if valid, otherwise a short error code such as "bad_plan_id" or "dup_job_id".
 */
inline const char* planValidate(const Plan& p) {
    if (!planTokenValid(p.id, kPlanIdLen)) return "bad_plan_id";
    if (!planTokenValid(p.vehicle, kPlanNameLen)) return "bad_vehicle";
    if (p.jobCount == 0) return "no_jobs";
    if (p.jobCount > kPlanMaxJobs) return "too_many_jobs";
    for (uint8_t i = 0; i < p.jobCount; i++) {
        const PlanJob& j = p.jobs[i];
        if (!planTokenValid(j.id, kPlanJobIdLen)) return "bad_job_id";
        for (uint8_t k = 0; k < i; k++) {
            if (std::strcmp(p.jobs[k].id, j.id) == 0) return "dup_job_id";
        }
        if (!planTokenValid(j.experiment, kPlanNameLen)) return "bad_experiment_name";
        if (j.media[0] && !planTokenValid(j.media, kPlanNameLen)) return "bad_media_name";
        if (j.repeat == 0 || j.repeat > kPlanMaxRepeat) return "bad_repeat";
        if (j.measureS > kPlanMaxMeasureS) return "bad_measure";
    }
    return nullptr;
}

/**
 * @brief Session scenario of a run: "fp_<plan>_<job>_r<rep>" (at most 28 characters).
 * @param p    Plan.
 * @param run  Zero-based run index.
 * @param out  Output buffer.
 * @param n    Size of out.
 * @return false if run is beyond the plan or out is too small.
 */
inline bool planScenario(const Plan& p, uint16_t run, char* out, size_t n) {
    uint8_t rep = 0;
    const PlanJob* j = planJobForRun(p, run, &rep);
    if (!j || !out || n == 0) return false;
    const int w = std::snprintf(out, n, "fp_%s_%s_r%u", p.id, j->id, static_cast<unsigned>(rep));
    return w > 0 && static_cast<size_t>(w) < n;
}

}  // namespace huprobe
