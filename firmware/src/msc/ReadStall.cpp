#include "msc/ReadStall.h"
#include "capture/OfflineCapture.h"
#include "core/ProbeClock.h"
#include "media/ProbeMedia.h"
#include "msc/MscBotTap.h"
#include "session/Session.h"
#include <cstring>

namespace {

portMUX_TYPE g_stallMux = portMUX_INITIALIZER_UNLOCKED;
constexpr uint16_t kMaxMarkers = 64;

const char* regionName(uint8_t r) {
    switch (r) {
        case ReadStall::kMeta: return "meta";
        case ReadStall::kHead: return "head";
        case ReadStall::kBody: return "body";
        default: return "?";
    }
}

void mark(Session* s, const char* name, const char* note, int64_t tUs) {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.capturing()) {
        oc.appendAutoMarker(tUs ? tUs : ProbeClock::nowUs(), name);
        return;
    }
    if (s && s->active()) s->marker(name, note ? note : "", tUs);
}

}  // namespace

ReadStall& ReadStall::instance() {
    static ReadStall r;
    return r;
}

bool ReadStall::configure(JsonVariantConst j, String* err) {
    Config c;
    JsonVariantConst reg = j["regions"];
    if (!reg.isNull()) {
        c.regions = 0;
        auto add = [&c](const char* s) {
            if (!s) return;
            if (strcmp(s, "meta") == 0) c.regions |= kMeta;
            else if (strcmp(s, "head") == 0) c.regions |= kHead;
            else if (strcmp(s, "body") == 0) c.regions |= kBody;
            else if (strcmp(s, "all") == 0) c.regions |= kMeta | kHead | kBody;
        };
        if (reg.is<JsonArrayConst>()) {
            for (JsonVariantConst v : reg.as<JsonArrayConst>()) add(v.as<const char*>());
        } else {
            add(reg.as<const char*>());
        }
        if (!c.regions) {
            if (err) *err = "regions_meta_head_body";
            return false;
        }
    }
    if (!j["file"].isNull() || !j["idx"].isNull()) {
        c.node = ProbeMedia::instance().resolveFile(j);
        if (c.node < 0) {
            if (err) *err = "unknown_file";
            return false;
        }
        if (c.regions & kMeta) {
            if (err) *err = "file_filter_excludes_meta";
            return false;
        }
    }
    c.delayMs = j["delay_ms"] | c.delayMs;
    if (c.delayMs == 0 || c.delayMs > kMaxDelayMs) {
        if (err) *err = "delay_ms_1_120000";
        return false;
    }
    const char* pace = j["pace"] | "fixed";
    if (strcmp(pace, "realtime") == 0) {
        c.paceFactor = j["pace_factor"] | 1.0f;
        if (c.paceFactor <= 0.0f || c.paceFactor > 10.0f) {
            if (err) *err = "pace_factor_0_10";
            return false;
        }
    } else if (strcmp(pace, "fixed") != 0) {
        if (err) *err = "pace_fixed_realtime";
        return false;
    }
    c.count = j["count"] | c.count;
    c.headBlocks = j["head_blocks"] | c.headBlocks;
    if (c.headBlocks == 0) c.headBlocks = 1;
    const float armS = j["arm_delay_s"] | 0.0f;
    const float winS = j["window_s"] | 0.0f;
    if (armS < 0 || armS > 600 || winS < 0 || winS > 3600) {
        if (err) *err = "arm_delay_s_0_600_window_s_0_3600";
        return false;
    }
    c.armDelayMs = (uint32_t)(armS * 1000.0f);
    c.windowMs = (uint32_t)(winS * 1000.0f);

    disable("reconfigure");
    portENTER_CRITICAL(&g_stallMux);
    cfg_ = c;
    held_ = abandoned_ = retries_ = 0;
    maxWaitMs_ = lastWaitMs_ = 0;
    doneHead_ = doneCount_ = 0;
    doneDropped_ = 0;
    decidedTag_ = MscBotTap::currentHostTag();  // never hold the command already in flight
    portEXIT_CRITICAL(&g_stallMux);
    markersEmitted_ = 0;
    enabledAtMs_ = millis();
    pendingStart_ = true;
    enabled_ = true;
    Serial.printf("[STALL] armed regions=0x%x node=%d delay=%ums count=%u arm=%ums window=%ums\n",
                  (unsigned)c.regions, (int)c.node, (unsigned)c.delayMs, (unsigned)c.count,
                  (unsigned)c.armDelayMs, (unsigned)c.windowMs);
    return true;
}

void ReadStall::disable(const char* reason) {
    if (!enabled_ && !active_ && !stalling_) return;
    enabled_ = false;
    active_ = false;
    pendingStart_ = false;
    portENTER_CRITICAL(&g_stallMux);
    // A held command is released on its next retry (decidedTag_ == its tag).
    stalling_ = false;
    portEXIT_CRITICAL(&g_stallMux);
    lastReason_ = reason ? reason : "off";
    offMarkPending_ = true;
    Serial.printf("[STALL] off (%s) held=%u abandoned=%u\n", lastReason_, (unsigned)held_,
                  (unsigned)abandoned_);
}

uint8_t ReadStall::classify(uint32_t lba, int16_t* node) const {
    uint32_t fblk = 0;
    const MediaLayout::Region r = ProbeMedia::instance().classify(lba, node, &fblk);
    switch (r) {
        case MediaLayout::Region::Boot:
        case MediaLayout::Region::Reserved:
        case MediaLayout::Region::Fat:
        case MediaLayout::Region::Root:
        case MediaLayout::Region::Dir:
            return kMeta;
        case MediaLayout::Region::File:
            return fblk < cfg_.headBlocks ? kHead : kBody;
        default:
            return 0;
    }
}

bool ReadStall::hold(uint32_t lba) {
    if (!active_ && !stalling_) return false;
    const uint32_t tag = MscBotTap::currentHostTag();
    portENTER_CRITICAL(&g_stallMux);
    if (stalling_) {
        if (tag == stallTag_) {
            const uint32_t waited = millis() - t0Ms_;
            if (waited < targetMs_) {
                retries_++;
                portEXIT_CRITICAL(&g_stallMux);
                vTaskDelay(pdMS_TO_TICKS(2));
                return true;
            }
            finishLocked(false);
            portEXIT_CRITICAL(&g_stallMux);
            return false;
        }
        finishLocked(true);
    }
    if (tag == decidedTag_) {
        portEXIT_CRITICAL(&g_stallMux);
        return false;
    }
    decidedTag_ = tag;
    portEXIT_CRITICAL(&g_stallMux);

    if (!active_) return false;
    int16_t node = -1;
    const uint8_t region = classify(lba, &node);
    if (!(region & cfg_.regions)) return false;
    if (cfg_.node >= 0 && node != cfg_.node) return false;

    const uint32_t bytes = MscBotTap::currentHostXferLen();
    uint32_t target = cfg_.delayMs;
    if (cfg_.paceFactor > 0.0f && node >= 0) {
        const uint16_t kbps = ProbeMedia::instance().layout().nodes[node].kbps;
        // bytes * 8 / (kbps * 1000) s = bytes * 8 / kbps ms
        if (kbps && bytes) target = (uint32_t)((float)bytes * 8.0f / kbps * cfg_.paceFactor);
    }
    if (target == 0) target = 1;
    if (target > kMaxDelayMs) target = kMaxDelayMs;

    const int64_t t0Us = ProbeClock::nowUs();
    portENTER_CRITICAL(&g_stallMux);
    stalling_ = true;
    stallTag_ = tag;
    targetMs_ = target;
    bytes_ = bytes;
    t0Ms_ = millis();
    t0Us_ = t0Us;
    lba_ = lba;
    node_ = node;
    region_ = region;
    portEXIT_CRITICAL(&g_stallMux);
    vTaskDelay(pdMS_TO_TICKS(2));
    return true;
}

void ReadStall::finishLocked(bool abandoned) {
    const uint32_t waited = millis() - t0Ms_;
    stalling_ = false;
    held_++;
    if (abandoned) abandoned_++;
    lastWaitMs_ = waited;
    if (waited > maxWaitMs_) maxWaitMs_ = waited;
    if (doneCount_ >= kDoneRing) {
        doneHead_ = (uint8_t)((doneHead_ + 1) % kDoneRing);
        doneCount_--;
        doneDropped_++;
    }
    Done& d = done_[(doneHead_ + doneCount_) % kDoneRing];
    d.t0Us = t0Us_;
    d.lba = lba_;
    d.waitMs = waited;
    d.targetMs = targetMs_;
    d.bytes = bytes_;
    d.node = node_;
    d.region = region_;
    d.abandoned = abandoned;
    doneCount_++;
    if (cfg_.count && held_ >= cfg_.count) active_ = false;
}

void ReadStall::onUsbReset() {
    portENTER_CRITICAL(&g_stallMux);
    if (stalling_) finishLocked(true);
    decidedTag_ = 0xFFFFFFFFu;
    portEXIT_CRITICAL(&g_stallMux);
}

void ReadStall::emit(const Done& d, Session* session) {
    if (markersEmitted_ >= kMaxMarkers) return;
    markersEmitted_++;
    char path[40] = "";
    if (d.node >= 0) ProbeMedia::instance().layout().nodePath((uint8_t)d.node, path, sizeof(path));
    char note[128];
    snprintf(note, sizeof(note), "lba=%u bytes=%u reg=%s file=%s target_ms=%u wait_ms=%u%s",
             (unsigned)d.lba, (unsigned)d.bytes, regionName(d.region), path,
             (unsigned)d.targetMs, (unsigned)d.waitMs, d.abandoned ? " abandoned" : "");
    mark(session, d.abandoned ? "stall_abandon" : "read_stall", note, d.t0Us);
    Serial.printf("[STALL] %s\n", note);
}

void ReadStall::poll(Session* session) {
    const uint32_t now = millis();
    const bool capturing = OfflineCapture::instance().capturing();
    if (enabled_ && capturedWhileEnabled_ && !capturing) disable("capture_end");
    if (!enabled_) capturedWhileEnabled_ = false;
    else if (capturing) capturedWhileEnabled_ = true;
    if (enabled_ && pendingStart_ && now - enabledAtMs_ >= cfg_.armDelayMs) {
        pendingStart_ = false;
        activeSinceMs_ = now;
        active_ = true;
        char note[48];
        snprintf(note, sizeof(note), "delay_ms=%u count=%u", (unsigned)cfg_.delayMs,
                 (unsigned)cfg_.count);
        mark(session, "stall_on", note, 0);
    }
    if (enabled_ && active_ && cfg_.windowMs && now - activeSinceMs_ >= cfg_.windowMs) {
        disable("window");
    }

    portENTER_CRITICAL(&g_stallMux);
    // Host moved on (new CBW visible) without retrying the held command.
    if (stalling_ && MscBotTap::currentHostTag() != stallTag_) finishLocked(true);
    Done batch[kDoneRing];
    uint8_t n = 0;
    while (doneCount_ > 0 && n < kDoneRing) {
        batch[n++] = done_[doneHead_];
        doneHead_ = (uint8_t)((doneHead_ + 1) % kDoneRing);
        doneCount_--;
    }
    const bool finished = enabled_ && !active_ && !stalling_ && cfg_.count && held_ >= cfg_.count;
    portEXIT_CRITICAL(&g_stallMux);

    for (uint8_t i = 0; i < n; i++) emit(batch[i], session);
    if (finished) disable("count");
    if (offMarkPending_) {
        offMarkPending_ = false;
        mark(session, "stall_off", lastReason_, 0);
    }
}

void ReadStall::regionsToJson(uint8_t regions, JsonArray a) {
    if (regions & kMeta) a.add("meta");
    if (regions & kHead) a.add("head");
    if (regions & kBody) a.add("body");
}

void ReadStall::toJson(JsonObject o) const {
    o["enabled"] = (bool)enabled_;
    o["active"] = (bool)active_;
    o["stalling"] = stalling_;
    regionsToJson(cfg_.regions, o["regions"].to<JsonArray>());
    o["node"] = cfg_.node;
    o["delay_ms"] = cfg_.delayMs;
    o["count"] = cfg_.count;
    o["head_blocks"] = cfg_.headBlocks;
    o["arm_delay_ms"] = cfg_.armDelayMs;
    o["window_ms"] = cfg_.windowMs;
    o["pace"] = cfg_.paceFactor > 0.0f ? "realtime" : "fixed";
    if (cfg_.paceFactor > 0.0f) o["pace_factor"] = cfg_.paceFactor;
    o["held"] = held_;
    o["abandoned"] = abandoned_;
    o["retries"] = retries_;
    o["last_wait_ms"] = lastWaitMs_;
    o["max_wait_ms"] = maxWaitMs_;
    o["dropped_events"] = doneDropped_;
    if (lastReason_ && lastReason_[0]) o["last_off"] = lastReason_;
}
