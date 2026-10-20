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
    // Trailing so older rows keep the defaults (false): unmount/unplug ends the capture.
    bool keepOnUnmount;       // default of the WebUI checkbox "Unmount beendet Aufnahme"
    bool freeMarkers;         // BOOT short = mark_NN, BOOT double = soft replug
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
static const char* const kMarkersStall[] = {
    "play_start", "menu_open", "menu_reaction", "audio_gap", "stop"};

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
     kMarkersStall, 5},
    {"s6_identity", "S6 Identity", "s6_identity", 60000, true, "media_hide",
     kMarkersGeneric, 5},
    {"s7_lifecycle", "S7 Lifecycle", "s7_lifecycle", 120000, true, "media_hide",
     kMarkersGeneric, 5},
    // Car default: already plugged into HU — no soft re-enum (umount would kill a short window).
    // keepOnUnmount=true → WebUI end_on_unmount unchecked; stop via timer / BOOT lang.
    {"free", "Freie Aufnahme (1 min, bis 30 min · BOOT 2x = Steckzyklus)", "free", 60000, false,
     "media_hide", nullptr, 0, true, true},
    // Drive mode (0.3.0): armed at boot with MSC still detached, so softUsb is the HU's
    // first enumeration. Ends at max duration, BOOT long or power loss (flash checkpoints).
    {"drive_home", "Heimfahrt automatisch (bis 30 min, Flash-Checkpoints)", "drive_home",
     1800000, true, "media_hide", nullptr, 0, true, true},
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

/**
 * Experiment sequences (0.2.31, R1). One step = media variant + base experiment
 * (markers, end strategy) + measure window + repetitions. SequenceRunner switches the
 * medium, prompts the driver (LED + WebUI), arms the offline capture and splits a step
 * into two half-length runs when the RAM buffer overflowed.
 */
enum class SeqSerial : uint8_t {
    Policy = 0,  // gadget default (new serial per re-enum, unless the preset pins one)
    Keep = 1,    // hold the current serial across re-enum (cache key = content only)
};

struct SequenceStep {
    const char* media;       // MediaPresets id
    const char* experiment;  // ExperimentProfile id (markers, end strategy)
    uint32_t measureMs;      // 0 = experiment default; capped to OfflineCapture max
    uint8_t repeats;
    SeqSerial serial;
    const char* prompt;      // driver instruction (UI/status)
    const char* stall;       // ReadStall JSON armed with the capture (arm_delay_s from capture start), or nullptr
    bool fineData;           // one DATA record per 4 KiB callback (chunk timing)
};

struct ExperimentSequence {
    const char* id;
    const char* label;
    const SequenceStep* steps;
    uint8_t stepCount;
};

namespace ExperimentSequencesDetail {
static const SequenceStep kSeqDur[] = {
    {"dur_mix", "s2_prefetch", 120000, 1, SeqSerial::Policy,
     "D010 waehlen, Autoplay laufen lassen (D010 -> D026 -> D060)"},
    {"dur_mix", "s2_prefetch", 150000, 1, SeqSerial::Policy,
     "D060 waehlen, bis zum Wechsel auf D180 laufen lassen"},
    {"dur_mix", "s2_prefetch", 180000, 1, SeqSerial::Policy,
     "D180 waehlen, bis zum Wechsel auf D010 laufen lassen"},
};
static const SequenceStep kSeqSize[] = {
    {"size_1_4_8", "s1_eager_eof", 90000, 1, SeqSerial::Policy,
     "S1M waehlen, bis zum Wechsel auf S4M laufen lassen", nullptr, true},
    {"size_1_4_8", "s1_eager_eof", 180000, 1, SeqSerial::Policy,
     "S4M waehlen, durchlaufen lassen (3 min)", nullptr, true},
    {"size_1_4_8", "s1_eager_eof", 180000, 1, SeqSerial::Policy,
     "S8M waehlen, durchlaufen lassen (3 min)", nullptr, true},
    {"pidrive_l3", "s1_eager_eof", 180000, 1, SeqSerial::Policy,
     "FAVORITE/01ROCK waehlen, durchlaufen lassen (3 min)", nullptr, true},
};
static const SequenceStep kSeqCache[] = {
    {"cache_a", "s3_cache_hit", 90000, 1, SeqSerial::Policy,
     "TRACK1 waehlen, Autoplay T1 -> T2 -> T3"},
    {"cache_a", "s3_cache_hit", 90000, 1, SeqSerial::Keep,
     "Gleicher Inhalt + gleiche Serial: TRACK1 waehlen, Autoplay"},
    {"cache_b", "s3_cache_hit", 90000, 1, SeqSerial::Keep,
     "Inhalt geaendert, gleiche Serial: TRACK1 waehlen, Autoplay"},
    {"ident_b", "s6_identity", 90000, 1, SeqSerial::Policy,
     "Inhalt A, neue Serial/Label/Volume-ID: TRACK1 waehlen, Autoplay"},
};
static const SequenceStep kSeqStruct[] = {
    {"folders_30", "s2_prefetch", 120000, 1, SeqSerial::Policy,
     "ALBUM1/T01 waehlen, Autoplay; nach 60 s Ordner ALBUM3 waehlen"},
    {"tail_probe", "s2_prefetch", 90000, 1, SeqSerial::Policy,
     "TRACK1 waehlen, Autoplay T1 -> T2 -> T3"},
    {"hdr_variants", "s2_prefetch", 150000, 1, SeqSerial::Policy,
     "H_PLAIN waehlen, Autoplay durch alle Header-Varianten"},
    {"frag_mix", "s2_prefetch", 180000, 1, SeqSerial::Policy,
     "F1 waehlen, Autoplay F1 -> F2 -> F3"},
};

// Stall ladder: one held READ10 per run (TRACK body, from 20 s after capture start).
// Steps 6/7 exceed the inferred >= 20 s per-command timeout on purpose: only parked.
static const SequenceStep kSeqStall[] = {
    {"cache_a", "s5_busy_budget", 75000, 1, SeqSerial::Policy,
     "STAND. TRACK1 waehlen (BOOT), Autoplay; Ton/Display beobachten (1 Read 2 s gebremst)",
     R"({"regions":"body","delay_ms":2000,"count":1,"arm_delay_s":20})"},
    {"cache_a", "s5_busy_budget", 75000, 1, SeqSerial::Policy,
     "STAND. TRACK1 waehlen, Autoplay (1 Read 5 s gebremst)",
     R"({"regions":"body","delay_ms":5000,"count":1,"arm_delay_s":20})"},
    {"cache_a", "s5_busy_budget", 90000, 1, SeqSerial::Policy,
     "STAND. TRACK1 waehlen, Autoplay (1 Read 10 s gebremst)",
     R"({"regions":"body","delay_ms":10000,"count":1,"arm_delay_s":20})"},
    {"cache_a", "s5_busy_budget", 100000, 1, SeqSerial::Policy,
     "STAND. TRACK1 waehlen, Autoplay (1 Read 20 s gebremst)",
     R"({"regions":"body","delay_ms":20000,"count":1,"arm_delay_s":20})"},
    {"cache_a", "s5_busy_budget", 120000, 1, SeqSerial::Policy,
     "STAND. TRACK1 waehlen, Autoplay (1 Read 30 s gebremst); Fehlermeldung notieren",
     R"({"regions":"body","delay_ms":30000,"count":1,"arm_delay_s":20})"},
    {"cache_a", "s5_busy_budget", 150000, 1, SeqSerial::Policy,
     "STAND. TRACK1 waehlen, Autoplay (1 Read 60 s gebremst); Fehlermeldung notieren",
     R"({"regions":"body","delay_ms":60000,"count":1,"arm_delay_s":20})"},
    {"cache_a", "s5_busy_budget", 180000, 1, SeqSerial::Policy,
     "STAND. TRACK1 waehlen, Autoplay (1 Read 120 s gebremst); Reaktion/Erholung notieren",
     R"({"regions":"body","delay_ms":120000,"count":1,"arm_delay_s":20})"},
    {"cache_a", "s5_busy_budget", 90000, 1, SeqSerial::Policy,
     "TRACK1 waehlen (BOOT); ab 10 s Dateiliste oeffnen (BOOT) und blaettern, BOOT wenn sie reagiert (FAT/DIR je 5 s)",
     R"({"regions":"meta","delay_ms":5000,"count":0,"arm_delay_s":10,"window_s":60})"},
    {"bitrate_mix", "s5_busy_budget", 120000, 1, SeqSerial::Policy,
     "B032 waehlen (BOOT); nach 20 s Dateiliste oeffnen (BOOT), BOOT wenn sie reagiert (Live-Tempo 32k)",
     R"({"regions":["head","body"],"pace":"realtime","count":0,"arm_delay_s":5,"window_s":100})"},
    {"bitrate_mix", "s5_busy_budget", 120000, 1, SeqSerial::Policy,
     "B128 waehlen (BOOT); nach 20 s Dateiliste oeffnen (BOOT), BOOT wenn sie reagiert (Live-Tempo 128k)",
     R"({"regions":["head","body"],"pace":"realtime","count":0,"arm_delay_s":5,"window_s":100})"},
    {"bitrate_mix", "s5_busy_budget", 120000, 1, SeqSerial::Policy,
     "B320 waehlen (BOOT); nach 20 s Dateiliste oeffnen (BOOT), BOOT wenn sie reagiert (Live-Tempo 320k)",
     R"({"regions":["head","body"],"pace":"realtime","count":0,"arm_delay_s":5,"window_s":100})"},
};

static const ExperimentSequence kTable[] = {
    {"seq_dur", "SEQ-DUR Dauer 10/26/60/180 s", kSeqDur, 3},
    {"seq_size", "SEQ-SIZE 1/4/8 MiB + PiDrive L3", kSeqSize, 4},
    {"seq_cache", "SEQ-CACHE Inhalt/Serial/Identitaet", kSeqCache, 4},
    {"seq_struct", "SEQ-STRUCT Ordner/Platzierung/Header/Fragmente", kSeqStruct, 4},
    {"seq_stall", "SEQ-STALL Timeout-Leiter / Menue / Live-Tempo (nur im Stand)", kSeqStall, 11},
};
}  // namespace ExperimentSequencesDetail

static const ExperimentSequence* const kSequences = ExperimentSequencesDetail::kTable;
static const size_t kSequenceCount =
    sizeof(ExperimentSequencesDetail::kTable) / sizeof(ExperimentSequencesDetail::kTable[0]);

inline const ExperimentSequence* findSequence(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kSequenceCount; i++) {
        if (strcmp(kSequences[i].id, id) == 0) return &kSequences[i];
    }
    return nullptr;
}
