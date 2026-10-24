#pragma once

/**
 * @file ExperimentProfiles.h
 * @brief Built-in experiment profiles and experiment sequences for offline HU measurements.
 */

#include <Arduino.h>
#include <cstring>

/**
 * @brief Experiment profiles for reproducible HU offline measurements (0.2.25+).
 *
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

/** @brief Storage for the experiment profile table and its operator marker lists. */
namespace ExperimentProfilesDetail {
/** @brief Operator marker list for S0 (cycled by BOOT short while WiFi is down). */
static const char* const kMarkersS0[] = {"plug_ready", "stop"};

/** @brief Operator marker list for S1 (eager EOF). */
static const char* const kMarkersS1[] = {"play_start", "audible_change", "stop"};

/** @brief Operator marker list for S2 (prefetch). */
static const char* const kMarkersS2[] = {
    "play_start", "track_next", "audible_change", "stop"};

/** @brief Operator marker list for S3 (cache/replay). */
static const char* const kMarkersS3[] = {
    "play_start", "audible_change", "stop", "replay", "audible_change", "stop"};

/** @brief Operator marker list for S4 (remount). */
static const char* const kMarkersS4[] = {
    "play_start", "audible_change", "stop", "replug", "play_start", "audible_change"};

/** @brief Operator marker list shared by S6 (identity) and S7 (lifecycle). */
static const char* const kMarkersGeneric[] = {
    "play_start", "track_next", "audible_change", "replay", "stop"};

/** @brief Operator marker list for S5 (busy budget / read stall). */
static const char* const kMarkersStall[] = {
    "play_start", "menu_open", "menu_reaction", "audio_gap", "stop"};

/** @brief All experiment profiles S0..S7, free capture and drive mode, in WebUI order. */
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

/** @brief Public view of the experiment profile table. */
static const ExperimentProfile* const kExperiments = ExperimentProfilesDetail::kTable;

/** @brief Number of entries in kExperiments. */
static const size_t kExperimentCount =
    sizeof(ExperimentProfilesDetail::kTable) / sizeof(ExperimentProfilesDetail::kTable[0]);

/**
 * @brief Look up an experiment profile by id (exact, case-sensitive match).
 * @param id  Profile id such as "s2_prefetch"; may be nullptr.
 * @return Pointer into kExperiments, or nullptr if @p id is nullptr or unknown.
 */
inline const ExperimentProfile* findExperiment(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kExperimentCount; i++) {
        if (strcmp(kExperiments[i].id, id) == 0) return &kExperiments[i];
    }
    return nullptr;
}

/**
 * @brief Look up an experiment profile by session scenario id.
 *
 * Relies on the scenario id being equal to the profile id (true for all built-in profiles).
 *
 * @param scenario  Session scenario id.
 * @return Matching profile, or nullptr if none has this id.
 */
inline const ExperimentProfile* findExperimentByScenario(const String& scenario) {
    return findExperiment(scenario.c_str());
}

/**
 * @brief Serial-number policy of a sequence step across USB re-enumerations.
 *
 * Experiment sequences (0.2.31, R1). One step = media variant + base experiment
 * (markers, end strategy) + measure window + repetitions. SequenceRunner switches the
 * medium, prompts the driver (LED + WebUI), arms the offline capture and splits a step
 * into two half-length runs when the RAM buffer overflowed.
 */
enum class SeqSerial : uint8_t {
    Policy = 0,  // gadget default (new serial per re-enum, unless the preset pins one)
    Keep = 1,    // hold the current serial across re-enum (cache key = content only)
};

/** @brief One step of an ExperimentSequence: media variant, base experiment, window and repeats. */
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

/** @brief Named, ordered list of SequenceStep entries run by SequenceRunner. */
struct ExperimentSequence {
    const char* id;
    const char* label;
    const SequenceStep* steps;
    uint8_t stepCount;
};

/** @brief Storage for the experiment sequence table and its step lists. */
namespace ExperimentSequencesDetail {
/** @brief SEQ-DUR steps: track duration mix (10/26/60/180 s). */
static const SequenceStep kSeqDur[] = {
    {"dur_mix", "s2_prefetch", 120000, 1, SeqSerial::Policy,
     "D010 waehlen, Autoplay laufen lassen (D010 -> D026 -> D060)"},
    {"dur_mix", "s2_prefetch", 150000, 1, SeqSerial::Policy,
     "D060 waehlen, bis zum Wechsel auf D180 laufen lassen"},
    {"dur_mix", "s2_prefetch", 180000, 1, SeqSerial::Policy,
     "D180 waehlen, bis zum Wechsel auf D010 laufen lassen"},
};

/** @brief SEQ-SIZE steps: 1/4/8 MiB files plus PiDrive L3, with fine DATA records. */
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

/**
 * @brief P0 Größenleiter (PiDrive Q16/B69): 158 KiB → 1 → 4 → 16 MiB.
 *
 * Per rung: Fresh (no mid-step cycle) then Cycle+Resume (BOOT double / Soft-Cycle).
 * freeMarkers: BOOT short = mark, BOOT double = soft replug. Capture 4 min (~180–300 s band).
 * fine_data on larger rungs for READ10/chunk timing; 16 MiB step notes start latency.
 */
static const SequenceStep kSeqSizeLadder[] = {
    {"size_158", "free", 240000, 1, SeqSerial::Keep,
     "FRESH 158k: Soft-Cycle vor Arm; TRACK1, 4 min Autoplay; KEIN weiterer Cycle",
     nullptr, true},
    {"size_158", "free", 240000, 1, SeqSerial::Keep,
     "CYCLE 158k: TRACK1; nach ~30s BOOT 2x Soft-Cycle; Resume; ggf. 2. Cycle",
     nullptr, true},
    {"size_1m", "free", 240000, 1, SeqSerial::Keep,
     "FRESH 1MiB: Soft-Cycle vor Arm; TRACK1 4 min; Mitten-Nachladen?",
     nullptr, true},
    {"size_1m", "free", 240000, 1, SeqSerial::Keep,
     "CYCLE 1MiB: TRACK1; ~30s BOOT 2x; Resume vs Fresh; ggf. 2. Cycle",
     nullptr, true},
    {"size_4m", "free", 240000, 1, SeqSerial::Keep,
     "FRESH 4MiB: Soft-Cycle vor Arm; TRACK1 4 min; Plateau/Readahead",
     nullptr, true},
    {"size_4m", "free", 240000, 1, SeqSerial::Keep,
     "CYCLE 4MiB: TRACK1; ~30s BOOT 2x; Resume; ggf. 2. Cycle",
     nullptr, true},
    {"size_16m", "free", 300000, 1, SeqSerial::Keep,
     "FRESH 16MiB: Soft-Cycle vor Arm; BOOT bei Wahl+hoerbar (Startlatenz); 5 min",
     nullptr, true},
    {"size_16m", "free", 300000, 1, SeqSerial::Keep,
     "CYCLE 16MiB: TRACK1; ~60s BOOT 2x; Resume+Nachladen; ggf. 2. Cycle",
     nullptr, true},
};

/** @brief SEQ-CACHE steps: content vs. serial vs. identity changes. */
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

/**
 * @brief SEQ-IDENT steps: Cache-Key / media identity (one dimension per step).
 *
 * Baseline pins Serial HP0A01 so Keep vs. Policy and label/vol-only are comparable.
 * In-session ID3/seed mutate (I1/I2) is not a sequence step — see feldplan.
 */
static const SequenceStep kSeqIdent[] = {
    {"cache_a_pin", "s3_cache_hit", 90000, 1, SeqSerial::Policy,
     "I0 Baseline HP0A01: TRACK1, Autoplay T1->T2->T3, dann Replay T1 (Hit?)"},
    {"cache_a_pin", "s3_cache_hit", 90000, 1, SeqSerial::Keep,
     "I0b Remount gleiche ID: TRACK1 Autoplay (Erwartung: full reread)"},
    {"ident_serial", "s6_identity", 90000, 1, SeqSerial::Policy,
     "I5 Nur Serial HP0S01: TRACK1; Resume/Re-Read + UI notieren"},
    {"cache_a_pin", "s3_cache_hit", 60000, 1, SeqSerial::Policy,
     "Baseline zurueck vor Label-Test: TRACK1 kurz Autoplay"},
    {"ident_label", "s6_identity", 90000, 1, SeqSerial::Keep,
     "I6 Nur Label HUPROBE_L (Serial Keep): TRACK1"},
    {"cache_a_pin", "s3_cache_hit", 60000, 1, SeqSerial::Policy,
     "Baseline zurueck vor Vol-Test: TRACK1 kurz Autoplay"},
    {"ident_vol", "s6_identity", 90000, 1, SeqSerial::Keep,
     "I7 Nur Volume-ID 0x48500701 (Serial Keep): TRACK1"},
    {"ident_b", "s6_identity", 90000, 1, SeqSerial::Policy,
     "I4 Alle Identity-Felder: TRACK1 Autoplay"},
};

/** @brief SEQ-STRUCT steps: folders, placement, header variants, fragmentation. */
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

/**
 * @brief SEQ-STALL steps: READ10 stall ladder, menu stall and live-tempo pacing.
 *
 * Stall ladder: one held READ10 per run (TRACK body, from 20 s after capture start).
 * Steps 6/7 exceed the inferred >= 20 s per-command timeout on purpose: only parked.
 */
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

/** @brief All built-in experiment sequences, in WebUI order. */
static const ExperimentSequence kTable[] = {
    {"seq_dur", "SEQ-DUR Dauer 10/26/60/180 s", kSeqDur, 3},
    {"seq_size", "SEQ-SIZE 1/4/8 MiB + PiDrive L3", kSeqSize, 4},
    {"seq_size_ladder", "SEQ-SIZE-P0 Leiter 158k/1/4/16 MiB Fresh+Cycle", kSeqSizeLadder, 8},
    {"seq_cache", "SEQ-CACHE Inhalt/Serial/Identitaet", kSeqCache, 4},
    {"seq_ident", "SEQ-IDENT Cache-Key Serial/Label/Vol (Stand)", kSeqIdent, 8},
    {"seq_struct", "SEQ-STRUCT Ordner/Platzierung/Header/Fragmente", kSeqStruct, 4},
    {"seq_stall", "SEQ-STALL Timeout-Leiter / Menue / Live-Tempo (nur im Stand)", kSeqStall, 11},
};
}  // namespace ExperimentSequencesDetail

/** @brief Public view of the experiment sequence table. */
static const ExperimentSequence* const kSequences = ExperimentSequencesDetail::kTable;

/** @brief Number of entries in kSequences. */
static const size_t kSequenceCount =
    sizeof(ExperimentSequencesDetail::kTable) / sizeof(ExperimentSequencesDetail::kTable[0]);

/**
 * @brief Look up an experiment sequence by id (exact, case-sensitive match).
 * @param id  Sequence id such as "seq_dur"; may be nullptr.
 * @return Pointer into kSequences, or nullptr if @p id is nullptr or unknown.
 */
inline const ExperimentSequence* findSequence(const char* id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < kSequenceCount; i++) {
        if (strcmp(kSequences[i].id, id) == 0) return &kSequences[i];
    }
    return nullptr;
}
