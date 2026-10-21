/**
 * @file PanicTail.cpp
 * @brief RTC no-init storage and JSON export for PanicTail.
 */
#include "core/PanicTail.h"
#include "core/DiagLog.h"

#include <ArduinoJson.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x48555055;  // HUPU (layout v2: 32 crumbs, restart cause)
constexpr uint8_t kCapacity = 32;

/** @brief One lifecycle breadcrumb (12 bytes). */
struct Breadcrumb {
    uint32_t ms;
    uint32_t arg;
    uint8_t code;
    uint8_t reserved;
    uint16_t repeat;  // extra identical crumbs folded into this one
};

/** @brief RTC-retained tail: session id, restart cause and a ring of kCapacity breadcrumbs. */
struct Tail {
    uint32_t magic;
    uint32_t generation;
    char sid[64];
    uint8_t next;
    uint8_t count;
    uint8_t restartCause;
    uint8_t reserved;
    Breadcrumb crumbs[kCapacity];
};

RTC_NOINIT_ATTR Tail retained;
Tail previous;
bool previousValid = false;
esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;

/**
 * @brief Map an ESP-IDF reset reason to a snake_case name.
 * @param reason  Reset reason from esp_reset_reason().
 * @return Static name, "unknown" for unmapped values.
 */
const char* reasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

/**
 * @brief Map a PanicTail::RestartCause value to a snake_case name.
 * @param cause  Stored restart cause.
 * @return Static name, "none" for RestartNone or unknown values.
 */
const char* causeName(uint8_t cause) {
    switch (cause) {
        case PanicTail::RestartWifiUnrecoverable: return "wifi_unrecoverable";
        case PanicTail::RestartPortalTimeout: return "wifi_portal_timeout";
        case PanicTail::RestartHubOta: return "hub_ota";
        case PanicTail::RestartWebUi: return "webui";
        case PanicTail::RestartGithubOta: return "github_ota";
        case PanicTail::RestartOtaRollback: return "ota_rollback";
        default: return "none";
    }
}

/**
 * @brief Decide whether identical consecutive crumbs of this code are folded.
 *
 * USB chatter repeats in bursts; folding keeps the lifecycle crumbs in the window.
 *
 * @param code  Breadcrumb code.
 * @return true for UsbMount, UsbUmount, EpStall, BotReset and NeedReset.
 */
bool foldable(uint8_t code) {
    return code == PanicTail::UsbMount || code == PanicTail::UsbUmount ||
           code == PanicTail::EpStall || code == PanicTail::BotReset ||
           code == PanicTail::NeedReset;
}

/**
 * @brief Map a breadcrumb code to a short snake_case name.
 * @param code  PanicTail::Code value.
 * @return Static name, "code_N" is avoided; unknown codes return "other".
 */
const char* crumbName(uint8_t code) {
    switch (code) {
        case PanicTail::SessionStart: return "session_start";
        case PanicTail::OfflineArm: return "offline_arm";
        case PanicTail::OfflineCapture: return "offline_capture";
        case PanicTail::MediaHide: return "media_hide";
        case PanicTail::SoftDetach: return "soft_detach";
        case PanicTail::WifiUpload: return "wifi_upload";
        case PanicTail::UsbMount: return "usb_mount";
        case PanicTail::UsbUmount: return "usb_umount";
        case PanicTail::BotReset: return "bot_reset";
        case PanicTail::NeedReset: return "need_reset";
        case PanicTail::EpStall: return "ep_stall";
        case PanicTail::MediaSwitch: return "media_switch";
        case PanicTail::UploadDone: return "upload_done";
        case PanicTail::MediaRestore: return "media_restore";
        case PanicTail::WifiLost: return "wifi_lost";
        case PanicTail::CoexHold: return "coex_hold";
        case PanicTail::CoexReattach: return "coex_reattach";
        case PanicTail::ZombieRestart: return "zombie_restart";
        case PanicTail::RestartRequested: return "restart";
        default: return "other";
    }
}

/**
 * @brief Append breadcrumbs from a tail into a JSON array (oldest first).
 * @param crumbs  Target array.
 * @param tail    Source tail (previous or current).
 */
void crumbsToJson(JsonArray crumbs, const Tail& tail) {
    const uint8_t first = (tail.next + kCapacity - tail.count) % kCapacity;
    for (uint8_t i = 0; i < tail.count; ++i) {
        const Breadcrumb& crumb = tail.crumbs[(first + i) % kCapacity];
        JsonObject item = crumbs.add<JsonObject>();
        item["code"] = crumb.code;
        item["name"] = crumbName(crumb.code);
        item["t_ms"] = crumb.ms;
        item["arg"] = crumb.arg;
        if (crumb.repeat) item["repeat"] = crumb.repeat;
    }
}
}  // namespace

void PanicTail::begin() {
    bootResetReason = esp_reset_reason();
    previousValid = retained.magic == kMagic && retained.count <= kCapacity;
    if (previousValid) memcpy(&previous, &retained, sizeof(previous));

    memset(&retained, 0, sizeof(retained));
    retained.magic = kMagic;
    retained.generation = previousValid ? previous.generation + 1 : 1;
}

void PanicTail::setSession(const char* sid) {
    if (!sid) return;
    snprintf(retained.sid, sizeof(retained.sid), "%s", sid);
    breadcrumb(SessionStart);
}

void PanicTail::breadcrumb(Code code, uint32_t arg) {
    if (retained.count > 0 && foldable(code)) {
        Breadcrumb& last = retained.crumbs[(retained.next + kCapacity - 1) % kCapacity];
        if (last.code == code && last.arg == arg) {
            if (last.repeat < 0xFFFF) last.repeat++;
            last.ms = millis();
            return;
        }
    }
    const uint8_t slot = retained.next % kCapacity;
    Breadcrumb& crumb = retained.crumbs[slot];
    crumb.ms = millis();
    crumb.arg = arg;
    crumb.code = static_cast<uint8_t>(code);
    crumb.repeat = 0;
    retained.next = (slot + 1) % kCapacity;
    if (retained.count < kCapacity) retained.count++;
}

void PanicTail::noteRestart(RestartCause cause) {
    retained.restartCause = static_cast<uint8_t>(cause);
    breadcrumb(RestartRequested, cause);
    DiagLog::persist("restart", true);
}

void PanicTail::appendPrevious(JsonObject obj) {
    appendBootInfo(obj);
    if (!previousValid || previous.count == 0) return;

    JsonObject tail = obj["panic_tail"].to<JsonObject>();
    tail["session_id"] = previous.sid;
    tail["generation"] = previous.generation;
    crumbsToJson(tail["breadcrumbs"].to<JsonArray>(), previous);
}

void PanicTail::appendCurrent(JsonObject obj) {
    JsonObject now = obj["panic_now"].to<JsonObject>();
    now["session_id"] = retained.sid;
    now["generation"] = retained.generation;
    now["count"] = retained.count;
    crumbsToJson(now["breadcrumbs"].to<JsonArray>(), retained);
}

size_t PanicTail::formatBreadcrumbs(char* dst, size_t n, bool fromPrevious) {
    if (!dst || n == 0) return 0;
    dst[0] = 0;
    const Tail* tail = fromPrevious ? (previousValid ? &previous : nullptr) : &retained;
    if (!tail || tail->count == 0) return 0;
    size_t used = 0;
    const uint8_t first = (tail->next + kCapacity - tail->count) % kCapacity;
    for (uint8_t i = 0; i < tail->count && used + 1 < n; ++i) {
        const Breadcrumb& crumb = tail->crumbs[(first + i) % kCapacity];
        int w = crumb.repeat
                    ? snprintf(dst + used, n - used, "  %lu %s arg=%lu x%u\n",
                               (unsigned long)crumb.ms, crumbName(crumb.code),
                               (unsigned long)crumb.arg, (unsigned)crumb.repeat)
                    : snprintf(dst + used, n - used, "  %lu %s arg=%lu\n",
                               (unsigned long)crumb.ms, crumbName(crumb.code),
                               (unsigned long)crumb.arg);
        if (w < 0) break;
        if (static_cast<size_t>(w) >= n - used) {
            dst[n - 1] = 0;
            return n - 1;
        }
        used += static_cast<size_t>(w);
    }
    return used;
}

void PanicTail::appendBootInfo(JsonObject obj) {
    obj["reset_reason"] = resetReason();
    obj["restart_cause"] = restartCause();
    obj["boot_generation"] = bootGeneration();
    obj["uptime_ms"] = (uint32_t)millis();
}

const char* PanicTail::resetReason() {
    return reasonName(bootResetReason);
}

const char* PanicTail::restartCause() {
    return previousValid ? causeName(previous.restartCause) : "none";
}

uint32_t PanicTail::bootGeneration() {
    return retained.generation;
}
