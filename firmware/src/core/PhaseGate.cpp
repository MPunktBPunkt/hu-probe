/**
 * @file PhaseGate.cpp
 * @brief RTC + NVS ENTER/PASS gates; DiagLog-friendly open-stack formatting.
 */

#include "core/PhaseGate.h"
#include "core/DiagLog.h"
#include "core/PanicTail.h"

#include <Preferences.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <cstring>

namespace {

constexpr uint32_t kMagic = 0x50484732;  // PHG2 (CRC-validated layout)
constexpr uint8_t kStackCap = 4;
constexpr const char* kNvsNs = "hpphase";

struct Frame {
    uint8_t phase;
    uint8_t reserved;
    uint16_t reserved2;
    uint32_t arg;
    uint32_t ms;
};

struct Gate {
    uint32_t magic;
    uint32_t generation;
    uint8_t depth;
    uint8_t reserved[3];
    Frame stack[kStackCap];
    uint8_t crashPhase;
    uint8_t crashDepth;
    uint16_t reserved3;
    uint32_t crashArg;
    uint32_t crashMs;
    uint8_t lastPassPhase;
    uint8_t reserved4[3];
    uint32_t lastPassMs;
    uint32_t lastPassArg;
    uint32_t crc;  // CRC of fields above (excluding crc itself)
};

RTC_NOINIT_ATTR Gate retained;

Gate previousCrash;
bool previousCrashValid = false;

uint32_t crc32Bytes(const uint8_t* data, size_t n) {
    uint32_t c = 0xFFFFFFFFUL;
    for (size_t i = 0; i < n; i++) {
        c ^= data[i];
        for (int b = 0; b < 8; b++) {
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        }
    }
    return ~c;
}

uint32_t gateCrc(const Gate& g) {
    return crc32Bytes(reinterpret_cast<const uint8_t*>(&g), offsetof(Gate, crc));
}

bool rtcValid() {
    return retained.magic == kMagic && retained.depth <= kStackCap &&
           retained.crc == gateCrc(retained);
}

void rtcCommit() {
    retained.magic = kMagic;
    retained.crc = gateCrc(retained);
}

bool isPowerLikeReset() {
    const esp_reset_reason_t r = esp_reset_reason();
    return r == ESP_RST_POWERON || r == ESP_RST_BROWNOUT || r == ESP_RST_EXT;
}

/** @brief Mirror open top to NVS (survives power_on). Logs on failure. */
bool nvsWriteOpen() {
    Preferences prefs;
    if (!prefs.begin(kNvsNs, false)) {
        DiagLog::printf("[PHASE] nvs write begin FAIL depth=%u\n", (unsigned)retained.depth);
        return false;
    }
    if (retained.depth == 0) {
        // Clean stack: clear live + sticky so the next boot is not a false crash.
        prefs.putUChar("valid", 0);
        prefs.putUChar("sticky", 0);
    } else {
        const Frame& top = retained.stack[retained.depth - 1];
        prefs.putUChar("valid", 1);
        prefs.putUChar("phase", top.phase);
        prefs.putUInt("arg", top.arg);
        prefs.putUInt("ms", top.ms);
        prefs.putUChar("depth", retained.depth);
        // Sticky copy of the latest open top (power_on recovery if valid was lost).
        prefs.putUChar("sticky", 1);
        prefs.putUChar("s_phase", top.phase);
        prefs.putUInt("s_arg", top.arg);
        prefs.putUInt("s_ms", top.ms);
        prefs.putUChar("s_depth", retained.depth);
    }
    prefs.end();
    return true;
}

void nvsClearOpen() {
    Preferences prefs;
    if (!prefs.begin(kNvsNs, false)) {
        DiagLog::printf("[PHASE] nvs clear begin FAIL\n");
        return;
    }
    prefs.putUChar("valid", 0);
    prefs.putUChar("sticky", 0);
    prefs.end();
}

bool nvsLoadCrash(uint8_t* phase, uint32_t* arg, uint32_t* ms, uint8_t* depth,
                  const char** via) {
    Preferences prefs;
    if (!prefs.begin(kNvsNs, true)) {
        DiagLog::printf("[PHASE] nvs load begin FAIL\n");
        return false;
    }
    // Prefer live open marker; fall back to sticky (last non-empty ENTER).
    bool ok = prefs.getUChar("valid", 0) == 1;
    if (ok) {
        *phase = prefs.getUChar("phase", 0);
        *arg = prefs.getUInt("arg", 0);
        *ms = prefs.getUInt("ms", 0);
        *depth = prefs.getUChar("depth", 1);
        *via = "nvs";
    } else if (prefs.getUChar("sticky", 0) == 1) {
        *phase = prefs.getUChar("s_phase", 0);
        *arg = prefs.getUInt("s_arg", 0);
        *ms = prefs.getUInt("s_ms", 0);
        *depth = prefs.getUChar("s_depth", 1);
        ok = (*phase != 0);
        *via = "nvs_sticky";
    }
    prefs.end();
    return ok && *phase != 0;
}

void latchCrash(uint8_t phase, uint32_t arg, uint32_t ms, uint8_t depth, const char* source) {
    retained.crashPhase = phase;
    retained.crashArg = arg;
    retained.crashMs = ms;
    retained.crashDepth = depth;
    previousCrash = retained;
    previousCrashValid = true;
    DiagLog::printf(
        "[PHASE] last_crash=%s arg=%u depth=%u t_ms=%u via=%s (open ENTER, no PASS)\n",
        PhaseGate::name(static_cast<PhaseGate::Phase>(phase)), (unsigned)arg, (unsigned)depth,
        (unsigned)ms, source ? source : "?");
    DiagLog::persist("phase", true);
    nvsClearOpen();
}

}  // namespace

const char* PhaseGate::name(Phase phase) {
    switch (phase) {
        case TripBatch: return "trip_batch";
        case TripReplay: return "trip_replay";
        case TripDrain: return "trip_drain";
        case TripTcpForce: return "trip_tcp_force";
        case TripWifiRe: return "trip_wifi_re";
        case StoredOcDrain: return "stored_oc_drain";
        case StoredQueuedClear: return "stored_queued_clear";
        case None:
        default: return "none";
    }
}

size_t PhaseGate::formatOpenLine(char* dst, size_t n) {
    if (!dst || n == 0) return 0;
    dst[0] = 0;
    if (!rtcValid() || retained.depth == 0) {
        return static_cast<size_t>(snprintf(dst, n, "# phase open=-\n"));
    }
    size_t used = 0;
    int w = snprintf(dst + used, n - used, "# phase open");
    if (w < 0) return 0;
    used += static_cast<size_t>(w);
    for (uint8_t i = 0; i < retained.depth && i < kStackCap && used + 1 < n; i++) {
        w = snprintf(dst + used, n - used, "%s%s:%u", i ? "," : "=",
                     name(static_cast<Phase>(retained.stack[i].phase)),
                     (unsigned)retained.stack[i].arg);
        if (w < 0) break;
        used += static_cast<size_t>(w);
    }
    if (used + 1 < n) {
        dst[used++] = '\n';
        dst[used] = 0;
    }
    return used;
}

void PhaseGate::begin() {
    previousCrashValid = false;
    memset(&previousCrash, 0, sizeof(previousCrash));

    const bool powerLike = isPowerLikeReset();
    const bool rtcOk = rtcValid();
    const char* nvsVia = nullptr;
    uint8_t nvsPh = 0, nvsDepth = 0;
    uint32_t nvsArg = 0, nvsMs = 0;
    const bool nvsOk = nvsLoadCrash(&nvsPh, &nvsArg, &nvsMs, &nvsDepth, &nvsVia);

    // power_on/brownout: RTC is unreliable — prefer NVS. Soft reset: RTC if CRC-valid.
    if (powerLike && nvsOk) {
        latchCrash(nvsPh, nvsArg, nvsMs, nvsDepth, nvsVia);
    } else if (!powerLike && rtcOk && retained.depth > 0) {
        const Frame& top = retained.stack[retained.depth - 1];
        latchCrash(top.phase, top.arg, top.ms, retained.depth, "rtc");
    } else if (nvsOk) {
        // Soft reset but empty RTC stack, or RTC CRC fail with NVS still set.
        latchCrash(nvsPh, nvsArg, nvsMs, nvsDepth, nvsVia ? nvsVia : "nvs");
    } else if (!rtcOk) {
        memset(&retained, 0, sizeof(retained));
    }

    retained.generation = rtcOk ? retained.generation + 1 : 1;
    retained.depth = 0;
    memset(retained.stack, 0, sizeof(retained.stack));
    if (!previousCrashValid) {
        retained.crashPhase = None;
        retained.crashDepth = 0;
        retained.crashArg = 0;
        retained.crashMs = 0;
    }
    retained.lastPassPhase = None;
    retained.lastPassArg = 0;
    retained.lastPassMs = 0;
    rtcCommit();

    DiagLog::printf("[PHASE] begin reset=%s rtc_ok=%d nvs_ok=%d crash=%s\n",
                    PanicTail::resetReason(), rtcOk ? 1 : 0, nvsOk ? 1 : 0,
                    previousCrashValid ? name(static_cast<Phase>(previousCrash.crashPhase))
                                       : "none");
}

void PhaseGate::enter(Phase phase, uint32_t arg) {
    if (phase == None) return;
    if (!rtcValid()) {
        memset(&retained, 0, sizeof(retained));
        retained.magic = kMagic;
        retained.generation = 1;
    }
    if (retained.depth >= kStackCap) {
        memmove(&retained.stack[0], &retained.stack[1], sizeof(Frame) * (kStackCap - 1));
        retained.depth = kStackCap - 1;
    }
    Frame& f = retained.stack[retained.depth++];
    f.phase = static_cast<uint8_t>(phase);
    f.arg = arg;
    f.ms = millis();
    rtcCommit();
    if (!nvsWriteOpen()) {
        DiagLog::printf("[PHASE] nvs mirror FAIL after ENTER %s\n", name(phase));
    }
    DiagLog::printf("[PHASE] ENTER %s arg=%u depth=%u\n", name(phase), (unsigned)arg,
                    (unsigned)retained.depth);
    // Force flash trail on the risky trip steps (USB-idle during stored upload).
    if (phase == TripBatch || phase == TripReplay || phase == StoredOcDrain) {
        DiagLog::persist("phase", true);
    }
}

void PhaseGate::pass(Phase phase) {
    if (phase == None || !rtcValid() || retained.depth == 0) return;
    Frame& top = retained.stack[retained.depth - 1];
    if (top.phase != static_cast<uint8_t>(phase)) {
        DiagLog::printf("[PHASE] PASS mismatch want=%s top=%s depth=%u\n", name(phase),
                        name(static_cast<Phase>(top.phase)), (unsigned)retained.depth);
        return;
    }
    retained.lastPassPhase = top.phase;
    retained.lastPassArg = top.arg;
    retained.lastPassMs = millis();
    retained.depth--;
    rtcCommit();
    nvsWriteOpen();
    DiagLog::printf("[PHASE] PASS %s arg=%u depth=%u\n", name(phase), (unsigned)top.arg,
                    (unsigned)retained.depth);
    if (phase == TripBatch) {
        DiagLog::persist("phase", false);
    }
}

void PhaseGate::finish(Phase phase) {
    if (phase == None) return;
    if (!rtcValid()) {
        nvsClearOpen();
        return;
    }
    while (retained.depth > 0) {
        const uint8_t topPh = retained.stack[retained.depth - 1].phase;
        if (topPh == static_cast<uint8_t>(phase)) {
            pass(phase);
            return;
        }
        DiagLog::printf("[PHASE] PASS auto-close %s arg=%u (finish %s)\n",
                        name(static_cast<Phase>(topPh)),
                        (unsigned)retained.stack[retained.depth - 1].arg, name(phase));
        retained.depth--;
        rtcCommit();
        nvsWriteOpen();  // keep NVS in sync during auto-close (was a wipe bug before)
    }
    nvsClearOpen();
}

PhaseGate::Phase PhaseGate::lastCrashPhase() {
    if (previousCrashValid) return static_cast<Phase>(previousCrash.crashPhase);
    if (rtcValid()) return static_cast<Phase>(retained.crashPhase);
    return None;
}

uint32_t PhaseGate::lastCrashArg() {
    if (previousCrashValid) return previousCrash.crashArg;
    if (rtcValid()) return retained.crashArg;
    return 0;
}

void PhaseGate::appendStatus(JsonObject obj) {
    JsonObject g = obj["phase_gate"].to<JsonObject>();
    g["generation"] = rtcValid() ? retained.generation : 0;

    JsonArray open = g["open"].to<JsonArray>();
    if (rtcValid()) {
        for (uint8_t i = 0; i < retained.depth && i < kStackCap; i++) {
            JsonObject f = open.add<JsonObject>();
            f["phase"] = name(static_cast<Phase>(retained.stack[i].phase));
            f["arg"] = retained.stack[i].arg;
            f["t_ms"] = retained.stack[i].ms;
        }
    }

    const Phase crash = lastCrashPhase();
    if (crash != None) {
        JsonObject c = g["last_crash"].to<JsonObject>();
        c["phase"] = name(crash);
        c["arg"] = lastCrashArg();
        if (previousCrashValid) {
            c["t_ms"] = previousCrash.crashMs;
            c["depth"] = previousCrash.crashDepth;
        } else {
            c["t_ms"] = retained.crashMs;
            c["depth"] = retained.crashDepth;
        }
    } else {
        g["last_crash"] = nullptr;
    }

    if (rtcValid() && retained.lastPassPhase != None) {
        JsonObject p = g["last_pass"].to<JsonObject>();
        p["phase"] = name(static_cast<Phase>(retained.lastPassPhase));
        p["arg"] = retained.lastPassArg;
        p["t_ms"] = retained.lastPassMs;
    }
}
