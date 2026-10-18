#pragma once

#include <Arduino.h>
#include <cstring>

/**
 * Experiment profiles for reproducible HU offline measurements (0.2.25+).
 * One button in the WebUI arms the matching offline capture; BOOT short cycles
 * the listed operator markers while WiFi is down.
 *
 * Epistemic rule (msc-host-read-semantics): without play_start +
 * (audible_change | track_next) → S2 = UNKNOWN.
 */
struct ExperimentProfile {
    const char* id;           // e.g. s2_prefetch
    const char* label;        // UI label
    const char* scenario;     // session scenario id (usually == id)
    uint32_t measureMs;       // timed end via media_hide / soft_detach
    bool softUsb;             // soft re-enum inside WiFi-off window
    const char* endStrategy;  // media_hide | soft_detach
    const char* const* markers;
    uint8_t markerCount;
};

namespace ExperimentProfilesDetail {
static const char* const kMarkersS0[] = {"plug_ready", "stop"};
static const char* const kMarkersS1[] = {"play_start", "audible_change", "stop"};
static const char* const kMarkersS2[] = {
    "play_start", "track_next", "audible_change", "stop"};
static const char* const kMarkersS3[] = {
    "play_start", "audible_change", "stop", "replay", "audible_change", "stop"};
static const char* const kMarkersS4[] = {
    "play_start", "audible_change", "stop", "replug", "play_start", "audible_change"};
static const char* const kMarkersGeneric[] = {
    "play_start", "track_next", "audible_change", "replay", "stop"};

static const ExperimentProfile kTable[] = {
    {"s0_plug_enum", "S0 Plug/Enum", "s0_plug_enum", 12000, true, "media_hide",
     kMarkersS0, 2},
    {"s1_eager_eof", "S1 Eager EOF", "s1_eager_eof", 45000, true, "media_hide",
     kMarkersS1, 3},
    {"s2_prefetch", "S2 Prefetch", "s2_prefetch", 60000, true, "media_hide",
     kMarkersS2, 4},
    {"s3_cache_hit", "S3 Cache/Replay", "s3_cache_hit", 90000, true, "media_hide",
     kMarkersS3, 6},
    {"s4_remount", "S4 Remount", "s4_remount", 90000, true, "soft_detach",
     kMarkersS4, 6},
    {"s5_busy_budget", "S5 Busy Budget", "s5_busy_budget", 120000, true, "media_hide",
     kMarkersGeneric, 5},
    {"s6_identity", "S6 Identity", "s6_identity", 60000, true, "media_hide",
     kMarkersGeneric, 5},
    {"s7_lifecycle", "S7 Lifecycle", "s7_lifecycle", 120000, true, "media_hide",
     kMarkersGeneric, 5},
};
}  // namespace ExperimentProfilesDetail

static const ExperimentProfile* const kExperiments = ExperimentProfilesDetail::kTable;
static const size_t kExperimentCount =
    sizeof(ExperimentProfilesDetail::kTable) / sizeof(ExperimentProfilesDetail::kTable[0]);

inline const ExperimentProfile* findExperiment(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kExperimentCount; i++) {
        if (strcmp(kExperiments[i].id, id) == 0) return &kExperiments[i];
    }
    return nullptr;
}

inline const ExperimentProfile* findExperimentByScenario(const String& scenario) {
    return findExperiment(scenario.c_str());
}
