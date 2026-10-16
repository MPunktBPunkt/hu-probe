#include "core/PanicTail.h"

#include <esp_attr.h>
#include <esp_system.h>
#include <cstring>

namespace {
constexpr uint32_t kMagic = 0x48555054;  // HUPT
constexpr uint8_t kCapacity = 8;

struct Breadcrumb {
    uint32_t ms;
    uint32_t arg;
    uint8_t code;
    uint8_t reserved[3];
};

struct Tail {
    uint32_t magic;
    uint32_t generation;
    char sid[64];
    uint8_t next;
    uint8_t count;
    uint8_t reserved[2];
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
}  // namespace

void PanicTail::begin() {
    bootResetReason = esp_reset_reason();
    previousValid = retained.magic == kMagic;
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
    const uint8_t slot = retained.next % kCapacity;
    Breadcrumb& crumb = retained.crumbs[slot];
    crumb.ms = millis();
    crumb.arg = arg;
    crumb.code = static_cast<uint8_t>(code);
    retained.next = (slot + 1) % kCapacity;
    if (retained.count < kCapacity) retained.count++;
}

void PanicTail::appendPrevious(JsonObject obj) {
    obj["reset_reason"] = resetReason();
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
    }
}

const char* PanicTail::resetReason() {
    return reasonName(bootResetReason);
}
