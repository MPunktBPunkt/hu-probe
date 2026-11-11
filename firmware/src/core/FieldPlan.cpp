/**
 * @file FieldPlan.cpp
 * @brief Field plan parsing, NVS cache and run counter.
 */
#include "core/FieldPlan.h"
#include "capture/ExperimentProfiles.h"
#include "core/DiagLog.h"
#include "core/GithubOta.h"
#include "media/MediaPresets.h"
#include <Preferences.h>

namespace FieldPlan {
namespace {

constexpr const char* kNs = "fplan";
constexpr size_t kMaxBody = 2048;

huprobe::Plan plan{};
bool valid = false;
uint16_t runsDone = 0;
bool fetched = false;
String vehicleId;
String lastResult = "not_checked";

/**
 * @brief Copy a JSON string member into a fixed buffer.
 * @return false if the member is missing, not a string or does not fit.
 */
bool copyStr(JsonVariantConst v, char* out, size_t cap, bool required) {
    out[0] = '\0';
    if (v.isNull()) return !required;
    if (!v.is<const char*>()) return false;
    const char* s = v.as<const char*>();
    if (strlen(s) >= cap) return false;
    strcpy(out, s);
    return true;
}

/**
 * @brief Parse and validate a plan body, including experiment and media lookup.
 * @param body      plan.json text.
 * @param[out] out  Parsed plan (only meaningful on success).
 * @param[out] err  Error code on failure.
 * @return true if the plan is usable.
 */
bool parse(const String& body, huprobe::Plan* out, String* err) {
    JsonDocument doc;
    if (deserializeJson(doc, body)) {
        *err = "parse";
        return false;
    }
    if ((doc["schema"] | 0) != 1) {
        *err = "schema";
        return false;
    }
    huprobe::Plan p{};
    p.enabled = doc["enabled"] | false;
    p.armAtHome = doc["arm_at_home"] | false;
    p.onlineOnly = doc["online_only"] | false;
    JsonArrayConst jobs = doc["jobs"].as<JsonArrayConst>();
    if (!copyStr(doc["plan"], p.id, sizeof(p.id), true) ||
        !copyStr(doc["vehicle"], p.vehicle, sizeof(p.vehicle), true) || jobs.isNull()) {
        *err = "fields";
        return false;
    }
    if (jobs.size() > huprobe::kPlanMaxJobs) {
        *err = "too_many_jobs";
        return false;
    }
    for (JsonVariantConst v : jobs) {
        JsonObjectConst j = v.as<JsonObjectConst>();
        huprobe::PlanJob& d = p.jobs[p.jobCount++];
        if (!copyStr(j["id"], d.id, sizeof(d.id), true) ||
            !copyStr(j["experiment"], d.experiment, sizeof(d.experiment), true) ||
            !copyStr(j["media"], d.media, sizeof(d.media), false)) {
            *err = "job_fields";
            return false;
        }
        d.fine = j["fine"] | false;
        d.measureS = j["measure_s"] | 0;
        const int repeat = j["repeat"] | 1;
        d.repeat = (repeat < 0 || repeat > 255) ? 0 : static_cast<uint8_t>(repeat);
        JsonVariantConst stall = j["stall"];
        if (!stall.isNull()) {
            if (!stall.is<JsonObjectConst>() || measureJson(stall) >= sizeof(d.stall)) {
                *err = "job_stall";
                return false;
            }
            serializeJson(stall, d.stall, sizeof(d.stall));
        }
    }
    if (const char* e = huprobe::planValidate(p)) {
        *err = String("invalid ") + e;
        return false;
    }
    for (uint8_t i = 0; i < p.jobCount; i++) {
        if (!findExperiment(p.jobs[i].experiment)) {
            *err = String("unknown_experiment ") + p.jobs[i].experiment;
            return false;
        }
        if (p.jobs[i].media[0] && !findMediaPreset(p.jobs[i].media)) {
            *err = String("unknown_media ") + p.jobs[i].media;
            return false;
        }
    }
    *out = p;
    return true;
}

/** @brief Persist the run counter in NVS. */
void saveRuns() {
    Preferences prefs;
    if (!prefs.begin(kNs, false)) return;
    prefs.putUShort("runs", runsDone);
    prefs.end();
}

}  // namespace

void begin(const String& vehicle) {
    vehicleId = vehicle;
    Preferences prefs;
    if (!prefs.begin(kNs, true)) return;
    const String body = prefs.getString("body", "");
    runsDone = prefs.getUShort("runs", 0);
    prefs.end();
    if (!body.length()) return;
    String err;
    valid = parse(body, &plan, &err);
    if (!valid) DiagLog::printf("[PLAN] cached plan rejected: %s\n", err.c_str());
}

void refresh(const String& url, String* info) {
    if (!url.length()) {
        lastResult = "off";
        if (info) *info = lastResult;
        return;
    }
    String body;
    String err;
    huprobe::Plan fresh{};
    if (!GithubOta::fetchSmall(url, kMaxBody, &body, &err) || !parse(body, &fresh, &err)) {
        lastResult = err;
    } else {
        const bool sameId = valid && strcmp(fresh.id, plan.id) == 0;
        plan = fresh;
        valid = true;
        fetched = true;
        Preferences prefs;
        if (prefs.begin(kNs, false)) {
            if (prefs.getString("body", "") != body) prefs.putString("body", body);
            if (!sameId) {
                runsDone = 0;
                prefs.putUShort("runs", 0);
            }
            prefs.end();
        }
        lastResult = String(sameId ? "same id=" : "new id=") + plan.id;
    }
    DiagLog::printf("[PLAN] %s (runs %u/%u)\n", lastResult.c_str(), (unsigned)runsDone,
                    valid ? (unsigned)huprobe::planTotalRuns(plan) : 0u);
    if (info) *info = lastResult;
}

bool active() {
    return valid && plan.enabled && vehicleId == plan.vehicle &&
           runsDone < huprobe::planTotalRuns(plan);
}

bool armAtHome() { return active() && plan.armAtHome; }

bool armBlocked() {
    return valid && plan.enabled && plan.onlineOnly && vehicleId == plan.vehicle && !fetched;
}

const huprobe::PlanJob* currentJob() {
    return active() ? huprobe::planJobForRun(plan, runsDone, nullptr) : nullptr;
}

bool currentScenario(char* out, size_t n) {
    return active() && huprobe::planScenario(plan, runsDone, out, n);
}

void advance() {
    if (!active()) return;
    runsDone++;
    saveRuns();
    DiagLog::printf("[PLAN] run %u/%u counted\n", (unsigned)runsDone,
                    (unsigned)huprobe::planTotalRuns(plan));
}

void toJson(JsonObject obj) {
    obj["result"] = lastResult;
    obj["active"] = active();
    if (!valid) return;
    obj["id"] = plan.id;
    obj["enabled"] = plan.enabled;
    obj["vehicle_ok"] = vehicleId == plan.vehicle;
    obj["arm_at_home"] = plan.armAtHome;
    obj["online_only"] = plan.onlineOnly;
    obj["fetched"] = fetched;
    obj["runs_done"] = runsDone;
    obj["runs_total"] = huprobe::planTotalRuns(plan);
    uint8_t rep = 0;
    if (const huprobe::PlanJob* j = huprobe::planJobForRun(plan, runsDone, &rep)) {
        obj["next_job"] = j->id;
        obj["next_rep"] = rep;
        obj["next_stall"] = j->stall[0] != '\0';
    }
}

}  // namespace FieldPlan
