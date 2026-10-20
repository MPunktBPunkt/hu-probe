#include "core/PanicTail.h"

#include <esp_attr.h>
#include <esp_system.h>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x48555055;  // HUPU (layout v2: 32 crumbs, restart cause)
constexpr uint8_t kCapacity = 32;

struct Breadcrumb {
    uint32_t ms;
    uint32_t arg;
    uint8_t code;
    uint8_t reserved;
    uint16_t repeat;  // extra identical crumbs folded into this one
};

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

/** USB chatter repeats in bursts; folding keeps the lifecycle crumbs in the window. */
bool foldable(uint8_t code) {
    return code == PanicTail::UsbMount || code == PanicTail::UsbUmount ||
           code == PanicTail::EpStall || code == PanicTail::BotReset ||
           code == PanicTail::NeedReset;
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
}

void PanicTail::appendPrevious(JsonObject obj) {
    appendBootInfo(obj);
    if (!previousValid || previous.count == 0) return;

    JsonObject tail = obj["panic_tail"].to<JsonObject>();
    tail["session_id"] = previous.sid;
    tail["generation"] = previous.generation;
    JsonArray crumbs = tail["breadcrumbs"].to<JsonArray>();
    const uint8_t first =
        (previous.next + kCapacity - previous.count) % kCapacity;
    for (uint8_t i = 0; i < previous.count; ++i) {
        const Breadcrumb& crumb = previous.crumbs[(first + i) % kCapacity];
        JsonObject item = crumbs.add<JsonObject>();
        item["code"] = crumb.code;
        item["t_ms"] = crumb.ms;
        item["arg"] = crumb.arg;
        if (crumb.repeat) item["repeat"] = crumb.repeat;
    }
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
