#include "capture/OfflineCapture.h"
#include "BuildFlags.h"
#include "core/EmitOrigin.h"
#include "core/HwInfo.h"
#include "core/ProbeClock.h"
#include "core/PanicTail.h"
#include "media/MediaLayout.h"
#include "msc/MscBotTap.h"
#include "net/EventBus.h"
#include "net/TcpStreamer.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <sys/time.h>
#include <time.h>
#include <climits>
#include <cstring>

namespace {

const char* const kRecFormat = "compact/1";
constexpr uint32_t kCbwSignature = 0x43425355;  // "USBC"
constexpr uint32_t kCswSignature = 0x53425355;  // "USBS"

void putLe16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void putLe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

uint16_t getLe16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

uint32_t getLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/** Length-prefixed string, at most maxLen chars. Returns bytes written. */
size_t putStr(uint8_t* p, const char* s, size_t maxLen) {
    const size_t n = s ? strnlen(s, maxLen) : 0;
    p[0] = (uint8_t)n;
    if (n) memcpy(p + 1, s, n);
    return 1 + n;
}

size_t getStr(const uint8_t* p, size_t avail, char* out, size_t outLen) {
    out[0] = 0;
    if (avail < 1) return 0;
    size_t n = p[0];
    if (n > avail - 1) n = avail - 1;
    const size_t take = n < outLen - 1 ? n : outLen - 1;
    memcpy(out, p + 1, take);
    out[take] = 0;
    return 1 + n;
}

}  // namespace

/**
 * Every kind has a fixed or self-describing payload length, so a corrupt length byte
 * (which would shift all following records) is caught instead of replayed as garbage.
 */
bool OfflineCapture::recShapeOk(uint8_t kind, const uint8_t* b, size_t bn) {
    switch (kind) {
        case 1:
            return bn >= 12 && b[11] <= 16 && bn == 12u + b[11];
        case kKindData: {
            if (bn < 23) return false;
            if (b[22] != kRegionInline) {
                return bn == 23 && b[22] <= static_cast<uint8_t>(MediaLayout::Region::Beyond);
            }
            size_t o = 23;
            for (int i = 0; i < 3; i++) {
                if (o >= bn) return false;
                o += 1u + b[o];
            }
            return o == bn;
        }
        case 3: return bn == 9;
        case 4:
        case 11: return bn == 0;
        case 5: return bn == 8;
        case 6:
        case 7:
        case kKindUmount: return bn == 2;
        case 9:
        case 10: return bn == 1;
        case kKindMarker: return bn < kMarkerNameLen;
        default: return false;
    }
}

bool OfflineCapture::timeFits(int64_t tUs) const {
    const int64_t d = tUs - tBaseUs_;
    return d >= INT32_MIN && d <= INT32_MAX;
}

uint8_t OfflineCapture::regionCode(const char* tag, const char* kind) {
    if (!tag || !kind) return kRegionInline;
    for (uint8_t r = 0; r <= static_cast<uint8_t>(MediaLayout::Region::Beyond); r++) {
        const MediaLayout::Region reg = static_cast<MediaLayout::Region>(r);
        if (strcmp(tag, MediaLayout::regionTag(reg)) == 0 &&
            strcmp(kind, MediaLayout::regionKind(reg)) == 0) {
            return r;
        }
    }
    return kRegionInline;
}

OfflineCapture& OfflineCapture::instance() {
    static OfflineCapture oc;
    return oc;
}

const char* OfflineCapture::stateName() const {
    switch (state_) {
        case State::Idle: return "idle";
        case State::Arming: return "arming";
        case State::Capturing: return "capturing";
        case State::Finalizing: return "finalizing";
        case State::Uploading: return "uploading";
    }
    return "?";
}

bool OfflineCapture::allocBuffer() {
    freeBuffer();
    // Prefer 64 KiB; fall back under fragmentation after long tcp_live sessions.
    static const size_t kTry[] = {64 * 1024, 48 * 1024, 32 * 1024, 24 * 1024};
    buf_ = nullptr;
    cap_ = 0;
    for (size_t i = 0; i < sizeof(kTry) / sizeof(kTry[0]); i++) {
        buf_ = static_cast<uint8_t*>(malloc(kTry[i]));
        if (buf_) {
            cap_ = kTry[i];
            break;
        }
    }
    if (!buf_) {
        cap_ = 0;
        return false;
    }
    used_ = 0;
    lastRecOff_ = kNoRec;
    tBaseUs_ = ProbeClock::nowUs();
    timeRangeDropped_ = 0;
    replayError_ = nullptr;
    replaySeq_ = 0;
    replayOff_ = 0;
    recordCount_ = 0;
    overflow_ = 0;
    postFinalizeRecords_ = 0;
    postFinalizeDropped_ = 0;
    Serial.printf("[OFFLINE] capture buffer %u bytes (free_heap=%u)\n", (unsigned)cap_,
                  (unsigned)ESP.getFreeHeap());
    return true;
}

void OfflineCapture::freeBuffer() {
    if (buf_) {
        free(buf_);
        buf_ = nullptr;
    }
    cap_ = used_ = 0;
}

bool OfflineCapture::wifiOff() {
    // Soft stop — WIFI_OFF/mode switch correlated with field reboots under OTG.
    Serial.println("[OFFLINE] esp_wifi_stop()");
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    delay(30);
    esp_err_t err = esp_wifi_stop();
    delay(100);
    Serial.printf("[OFFLINE] wifi_stop err=%d\n", (int)err);
    return true;
}

bool OfflineCapture::wifiOn() {
    Serial.println("[OFFLINE] WiFi ON (esp_wifi_start + begin)");
    WiFi.mode(WIFI_STA);
    esp_wifi_start();
    delay(50);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setAutoReconnect(true);
    WiFi.begin();  // NVS creds from WiFiManager
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 25000) {
        delay(200);
        yield();
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OFFLINE] WiFi reconnect FAILED");
        return false;
    }
    Serial.printf("[OFFLINE] WiFi %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void OfflineCapture::reconnectWifiThrottled() {
    // Association + DHCP needs several seconds; WiFi.reconnect() on every loop pass aborts
    // each attempt (the old Drain loop never reconnected → frozen purple until RST).
    unsigned long now = millis();
    if (lastWifiReconnectMs_ != 0 && now - lastWifiReconnectMs_ < kWifiReconnectEveryMs) return;
    lastWifiReconnectMs_ = now;
    Serial.println("[OFFLINE] WiFi lost during upload — reconnect");
    WiFi.reconnect();
}

void OfflineCapture::pumpTcp(uint32_t maxMs) {
    if (!tcp_) return;
    unsigned long t0 = millis();
    while (millis() - t0 < maxMs) {
        tcp_->loop();
        yield();
        delay(10);
        if (EventBus::instance().pending() == 0 && tcp_->connected()) break;
    }
}

/** Drain ring during replay so a 254+ Rec capture cannot drop before WiFi TX. */
void OfflineCapture::pumpTcpWhilePending(uint32_t maxMs, size_t keepBelow) {
    if (!tcp_) return;
    if (EventBus::instance().pending() < keepBelow) return;
    unsigned long t0 = millis();
    while (millis() - t0 < maxMs) {
        tcp_->loop();
        yield();
        delay(5);
        if (EventBus::instance().pending() < keepBelow && tcp_->connected()) break;
        if (EventBus::instance().pending() == 0) break;
    }
}

bool OfflineCapture::arm(Session* session, MscGadget* msc, ReadInstrument* inst, TcpStreamer* tcp) {
    if (active()) return false;
    session_ = session;
    msc_ = msc;
    inst_ = inst;
    tcp_ = tcp;
    state_ = State::Arming;
    sessionStarted_ = false;
    EmitOrigin::clear();
    stopRequested_ = false;
    stopReason_ = "";
    uploadStatus_ = "arming";
    mediaHidden_ = false;
    fallbackDetached_ = false;
    wasPlugged_ = false;
    unplugSinceMs_ = 0;

    if (session_ && session_->active()) session_->stop();
    // Free EventBus String heap before large capture malloc (OOM→reboot risk).
    EventBus::instance().clear();
    delay(50);
    EventBus::instance().clear();
    delay(150);
    armFreeHeap_ = ESP.getFreeHeap();
    armMaxAlloc_ = ESP.getMaxAllocHeap();
    Serial.printf("[OFFLINE] pre-alloc free_heap=%u max_alloc=%u\n", (unsigned)armFreeHeap_,
                  (unsigned)armMaxAlloc_);

    wallclock_[0] = 0;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    anchorCaptureUs_ = ProbeClock::nowUs();
    anchorUnixUs_ = tv.tv_sec > 100000 ? (int64_t)tv.tv_sec * 1000000LL + tv.tv_usec : 0;
    time_t now = time(nullptr);
    struct tm tm;
    if (now > 100000 && gmtime_r(&now, &tm)) {
        snprintf(wallclock_, sizeof(wallclock_), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                 tm.tm_sec);
    } else {
        snprintf(wallclock_, sizeof(wallclock_), "boot+%lu", (unsigned long)millis());
    }

    if (!allocBuffer()) {
        Serial.println("[OFFLINE] malloc capture buffer failed");
        state_ = State::Idle;
        uploadStatus_ = "buffer_alloc_failed";
        return false;
    }
    if (!wifiOff()) {
        freeBuffer();
        state_ = State::Idle;
        return false;
    }
    // Settling so assoc teardown does not overlap MSC.
    delay(500);
    PanicTail::breadcrumb(PanicTail::OfflineArm);
    Serial.printf("[OFFLINE] armed wallclock=%s buf=%u\n", wallclock_, (unsigned)cap_);
    return true;
}

void OfflineCapture::setMarkerSequence(const char* const* markers, uint8_t count) {
    markerCount_ = 0;
    markerIndex_ = 0;
    lastMarker_[0] = 0;
    if (!markers || count == 0) {
        // Default S2-style operator markers (audit: play_start + audible/track).
        static const char* const kDefault[] = {
            "play_start", "track_next", "audible_change", "replay", "stop"};
        markers = kDefault;
        count = 5;
    }
    if (count > kMaxMarkers) count = kMaxMarkers;
    for (uint8_t i = 0; i < count; i++) {
        if (!markers[i]) continue;
        snprintf(markerSeq_[markerCount_], kMarkerNameLen, "%s", markers[i]);
        markerCount_++;
    }
}

void OfflineCapture::queueNextExperimentMarker() {
    if (!capturing() || markerCount_ == 0) return;
    const char* name = markerSeq_[markerIndex_ % markerCount_];
    markerIndex_++;
    snprintf(lastMarker_, sizeof(lastMarker_), "%s", name);
    appendMarkerRec(ProbeClock::nowUs(), name);
    Serial.printf("[OFFLINE] marker %s (%u/%u)\n", name, markerIndex_, markerCount_);
}

void OfflineCapture::queueFreeMarker(int64_t tUs) {
    freeMarkIdx_++;
    snprintf(lastMarker_, sizeof(lastMarker_), "mark_%02u", (unsigned)freeMarkIdx_);
    appendMarkerRec(tUs, lastMarker_);
    Serial.printf("[OFFLINE] marker %s\n", lastMarker_);
}

void OfflineCapture::softReplug() {
    if (!msc_) return;
    softReplugs_++;
    snprintf(lastMarker_, sizeof(lastMarker_), "soft_replug");
    appendMarkerRec(ProbeClock::nowUs(), lastMarker_);
    // D+ only: power stays, the HU sees detach -> attach. Must not end the capture even if
    // "unmount ends capture" is on.
    ignoreUnplugUntilMs_ = millis() + kSoftReplugHoldMs + 2500;
    msc_->softCycle(kSoftReplugHoldMs);
    Serial.printf("[OFFLINE] soft replug #%u hold=%ums\n", (unsigned)softReplugs_,
                  (unsigned)kSoftReplugHoldMs);
}

void OfflineCapture::saveLastRun(bool complete, uint32_t dropped) {
    lastRun_.valid = true;
    snprintf(lastRun_.wallclock, sizeof(lastRun_.wallclock), "%s", wallclock_);
    snprintf(lastRun_.vehicle, sizeof(lastRun_.vehicle), "%s", vehicle_.c_str());
    snprintf(lastRun_.scenario, sizeof(lastRun_.scenario), "%s", scenario_.c_str());
    snprintf(lastRun_.stopReason, sizeof(lastRun_.stopReason), "%s", stopReason_.c_str());
    snprintf(lastRun_.uploadStatus, sizeof(lastRun_.uploadStatus), "%s",
             uploadStatus_.c_str());
    snprintf(lastRun_.endStrategy, sizeof(lastRun_.endStrategy), "%s",
             endStrategy_.c_str());
    lastRun_.records = recordCount_;
    lastRun_.bytes = (uint32_t)used_;
    lastRun_.cap = (uint32_t)cap_;
    lastRun_.overflow = overflow_;
    lastRun_.dropped = dropped;
    lastRun_.measureMs = autoDetachMs_;
    lastRun_.complete = complete;
    // Probe-side gate (collector still checks recv_seq_gaps). Overflow/drops → FAIL.
    lastRun_.integrityPass = (overflow_ == 0) && (dropped == 0) && complete;
}

bool OfflineCapture::beginCapture(const String& vehicle, const String& scenario,
                                  const String& profileId, bool softUsb,
                                  uint32_t autoDetachMs, const String& endStrategy,
                                  const char* const* markers, uint8_t markerCount,
                                  bool endOnUnmount, bool freeMarkers) {
    if (state_ != State::Arming && state_ != State::Idle) return false;
    if (state_ == State::Idle) return false;
    endOnUnmount_ = endOnUnmount;
    freeMarkers_ = freeMarkers;
    freeMarkIdx_ = 0;
    softReplugs_ = 0;
    pendingTapMs_ = 0;
    vehicle_ = vehicle;
    scenario_ = scenario;
    profileId_ = profileId;
    softUsb_ = softUsb;
    autoDetachMs_ = autoDetachMs;
    endStrategy_ = (endStrategy == "soft_detach") ? "soft_detach" : "media_hide";
    setMarkerSequence(markers, markerCount);
    if (freeMarkers_) markerCount_ = 0;
    autoDetached_ = false;
    mediaHidden_ = false;
    fallbackDetached_ = false;
    uploadStatus_ = "capturing";
    transportEpochStart_ = msc_ ? msc_->transportEpoch() : 0;
    transportEpochEnd_ = transportEpochStart_;
    stopRequested_ = false;
    stopReason_ = "";
    captureStartedMs_ = millis();
    wasPlugged_ = false;  // set after re-enum — else softDetach looks like host unplug
    state_ = State::Capturing;

    if (softUsb_ && msc_) {
        // Clean re-enum entirely inside the WiFi-off window (Messkonzept ARMING).
        // Soft-detach produces umount — must not end the capture.
        ignoreUnplugUntilMs_ = millis() + 2500;
        Serial.println("[OFFLINE] soft USB re-enum");
        msc_->softDetach(0);
        delay(400);
        msc_->softAttach();
        delay(200);
        wasPlugged_ = msc_->plugged();
        // Drop a spurious stop from umount during re-enum.
        if (stopRequested_ && stopReason_ == "umount") {
            stopRequested_ = false;
            stopReason_ = "";
        }
    } else {
        ignoreUnplugUntilMs_ = 0;
        wasPlugged_ = msc_ && msc_->plugged();
    }

    PanicTail::breadcrumb(PanicTail::OfflineCapture, transportEpochStart_);
    Serial.printf("[OFFLINE] capturing vehicle=%s scenario=%s soft_usb=%d auto_detach_ms=%u end=%s\n",
                  vehicle_.c_str(), scenario_.c_str(), softUsb_ ? 1 : 0, autoDetachMs_,
                  endStrategy_.c_str());
    return true;
}

void OfflineCapture::requestStop(const char* reason) {
    if (!capturing()) return;
    stopRequested_ = true;
    stopReason_ = reason ? reason : "stop";
}

void OfflineCapture::queueMarker(const char* name) {
    if (!capturing()) return;
    const char* n = name && name[0] ? name : "operator";
    snprintf(lastMarker_, sizeof(lastMarker_), "%s", n);
    appendMarkerRec(ProbeClock::nowUs(), n);
}

void OfflineCapture::appendAutoMarker(int64_t tUs, const char* name) {
    if (!acceptsPending()) return;
    if (capturing()) {
        appendMarkerRec(tUs, name);
        return;
    }
    const size_t n = name ? strnlen(name, kMarkerNameLen - 1) : 0;
    uint8_t* p = reserveRec(kRecHdr + n, true, tUs);
    if (!p) return;
    writeHdr(p, kKindMarker, (uint8_t)(kRecHdr + n), true, 0, tUs);
    if (n) memcpy(p + kRecHdr, name, n);
}

void OfflineCapture::appendMarkerRec(int64_t tUs, const char* name) {
    const size_t n = name ? strnlen(name, kMarkerNameLen - 1) : 0;
    uint8_t* p = reserveRec(kRecHdr + n, false, tUs);
    if (!p) return;
    writeHdr(p, kKindMarker, (uint8_t)(kRecHdr + n), false, 0, tUs);
    if (n) memcpy(p + kRecHdr, name, n);
}

uint8_t* OfflineCapture::reserveRec(size_t n, bool post, int64_t tUs) {
    if (!timeFits(tUs)) {
        // Never store a clamped (wrong) time; the loss is counted and fails integrity.
        timeRangeDropped_++;
        if (post) postFinalizeDropped_++;
        return nullptr;
    }
    if (post) {
        // Diagnostic tail after media_hide: never counts against capture integrity.
        if (postFinalizeRecords_ >= kMaxPostFinalizeRecs || used_ + n > cap_) {
            postFinalizeDropped_++;
            return nullptr;
        }
        postFinalizeRecords_++;
    } else {
        if (used_ + n > cap_) {
            overflow_++;
            return nullptr;
        }
        recordCount_++;
    }
    uint8_t* p = buf_ + used_;
    lastRecOff_ = used_;
    used_ += n;
    return p;
}

void OfflineCapture::writeHdr(uint8_t* p, uint8_t kind, uint8_t len, bool post, uint8_t flag,
                              int64_t tUs) const {
    p[0] = kind;
    p[1] = len;
    p[2] = (uint8_t)((post ? 0x80 : 0) | (flag & 0x7F));
    putLe32(p + 3, (uint32_t)(int32_t)(tUs - tBaseUs_));  // range checked by reserveRec
}

bool OfflineCapture::appendPending(uint8_t kind, int64_t tUs, const uint8_t* raw31,
                                   uint32_t botTag, uint32_t lba, uint32_t nBytes,
                                   uint32_t durUs, uint32_t gapUs, uint16_t wValue,
                                   uint16_t wIndex, uint16_t wLength, uint8_t bmRequestType,
                                   uint8_t bRequest, uint8_t epAddr, uint8_t flag,
                                   const char* tag, const char* kindStr, const char* dir) {
    if (!acceptsPending()) return false;
    const bool post = !capturing();
    // Coalesce contiguous DATA chunks of one CBW (4 KiB callbacks) into one record:
    // an 8 MiB read would otherwise need ~2048 records.
    if (kind == kKindData && lastRecOff_ != kNoRec && buf_[lastRecOff_] == kKindData) {
        uint8_t* last = buf_ + lastRecOff_;
        const int64_t lastT = tBaseUs_ + (int32_t)getLe32(last + 3);
        const uint32_t lastBytes = getLe32(last + kRecHdr + 8);
        const uint16_t chunks = getLe16(last + kRecHdr + 20);
        if (((last[2] & 0x80) != 0) == post && getLe32(last + kRecHdr) == botTag &&
            getLe32(last + kRecHdr + 4) + lastBytes / 512u == lba && (lastBytes % 512u) == 0 &&
            chunks < 0xFFFF && tUs >= lastT) {
            putLe32(last + kRecHdr + 8, lastBytes + nBytes);
            const int64_t span = tUs + (int64_t)durUs - lastT;
            putLe32(last + kRecHdr + 12, (span > 0xFFFFFFFFLL) ? 0xFFFFFFFFu : (uint32_t)span);
            putLe16(last + kRecHdr + 20, (uint16_t)(chunks + 1));
            return true;
        }
    }

    uint8_t body[kRecMax - kRecHdr];
    size_t n = 0;
    switch (kind) {
        case 1: {  // CBW minus its constant signature; CDB up to the last non-zero byte
            if (!raw31) return false;
            memcpy(body, raw31 + 4, 11);  // tag, xfer_len, flags, lun, cdb_len
            uint8_t cdb = 16;
            while (cdb > 0 && raw31[15 + cdb - 1] == 0) cdb--;
            body[11] = cdb;
            memcpy(body + 12, raw31 + 15, cdb);
            n = 12u + cdb;
            break;
        }
        case kKindData: {
            putLe32(body, botTag);
            putLe32(body + 4, lba);
            putLe32(body + 8, nBytes);
            putLe32(body + 12, durUs);
            putLe32(body + 16, gapUs);
            putLe16(body + 20, 1);  // coalesced chunk count
            const uint8_t region = regionCode(tag, kindStr);
            const bool dirIn = dir && strcmp(dir, "in") == 0;
            n = 23;
            if (region != kRegionInline && dirIn) {
                body[22] = region;
            } else {
                body[22] = kRegionInline;
                n += putStr(body + n, tag, 7);
                n += putStr(body + n, kindStr, 7);
                n += putStr(body + n, dir, 3);
            }
            break;
        }
        case 3:  // CSW minus signature: tag, residue, status
            if (!raw31) return false;
            memcpy(body, raw31 + 4, 9);
            n = 9;
            break;
        case 5:
            body[0] = bmRequestType;
            body[1] = bRequest;
            putLe16(body + 2, wValue);
            putLe16(body + 4, wIndex);
            putLe16(body + 6, wLength);
            n = 8;
            break;
        case 6:
        case 7:
        case 8:
            putLe16(body, wValue);
            n = 2;
            break;
        case 9:
        case 10:
            body[0] = epAddr;
            n = 1;
            break;
        case 4:
        case 11:
            break;
        default:
            // A new MscBotTap::Kind needs an encoding here and in recShapeOk/replayOneRec.
            return false;
    }
    uint8_t* p = reserveRec(kRecHdr + n, post, tUs);
    if (!p) return false;
    writeHdr(p, kind, (uint8_t)(kRecHdr + n), post, flag, tUs);
    if (n) memcpy(p + kRecHdr, body, n);
    if (post) return true;
    // Host unmount ends the window — but not during soft re-enum grace.
    if (kind == kKindUmount && endOnUnmount_ && capturing() && millis() >= ignoreUnplugUntilMs_) {
        requestStop("umount");
    }
    return true;
}

bool OfflineCapture::replayFail(const char* why) {
    replayError_ = why;
    Serial.printf("[OFFLINE] replay stopped: %s at byte %u (record %u)\n", why,
                  (unsigned)replayOff_, (unsigned)replaySeq_);
    return false;
}

bool OfflineCapture::replayComplete() const {
    return !replayError_ && replayOff_ == used_ &&
           replaySeq_ == recordCount_ + postFinalizeRecords_;
}

bool OfflineCapture::replayOneRec() {
    if (!inst_ || !buf_ || !session_ || !sessionStarted_) return false;
    if (replayOff_ == used_) {
        if (replaySeq_ != recordCount_ + postFinalizeRecords_) return replayFail("count_mismatch");
        return false;
    }
    if (replayOff_ + kRecHdr > used_) return replayFail("truncated_header");
    const uint8_t* p = buf_ + replayOff_;
    const uint8_t len = p[1];
    if (len < kRecHdr || replayOff_ + len > used_) return replayFail("bad_length");
    if (!recShapeOk(p[0], p + kRecHdr, len - kRecHdr)) return replayFail("bad_shape");
    replayOff_ += len;
    const uint32_t captureSeq = replaySeq_++;
    const uint8_t kind = p[0];
    const bool post = (p[2] & 0x80) != 0;
    const uint8_t flag = p[2] & 0x7F;
    const int64_t tUs = tBaseUs_ + (int32_t)getLe32(p + 3);
    const uint8_t* b = p + kRecHdr;
    const size_t bn = len - kRecHdr;
    const char* ser = msc_ ? msc_->identitySerial() : nullptr;
    // B7: unclamped capture time + buffer index; live tail keeps "post_finalize".
    EmitOrigin::set(post ? "post_finalize_buffered" : "offline", true, true, captureSeq);
    if (kind == kKindMarker) {
        char name[kMarkerNameLen];
        const size_t n = bn < sizeof(name) - 1 ? bn : sizeof(name) - 1;
        memcpy(name, b, n);
        name[n] = 0;
        session_->marker(name[0] ? name : "operator", "offline", tUs);
        EmitOrigin::set("post_finalize", true);
        return true;
    }
    switch (kind) {
        case 1: {
            if (bn < 12 || b[11] > 16 || bn < 12u + b[11]) break;
            uint8_t raw[31] = {0};
            putLe32(raw, kCbwSignature);
            memcpy(raw + 4, b, 11);
            memcpy(raw + 15, b + 12, b[11]);
            inst_->emitBotCbwTinyusb(raw, tUs);
            break;
        }
        case 2: {
            if (bn < 23) break;
            char tag[8] = "";
            char kindStr[8] = "";
            char dir[4] = "in";
            if (b[22] == kRegionInline) {
                size_t o = 23;
                o += getStr(b + o, bn - o, tag, sizeof(tag));
                o += getStr(b + o, bn - o, kindStr, sizeof(kindStr));
                getStr(b + o, bn - o, dir, sizeof(dir));
            } else {
                const MediaLayout::Region r = static_cast<MediaLayout::Region>(b[22]);
                snprintf(tag, sizeof(tag), "%s", MediaLayout::regionTag(r));
                snprintf(kindStr, sizeof(kindStr), "%s", MediaLayout::regionKind(r));
            }
            inst_->setReplayChunks(getLe16(b + 20));
            inst_->emitBotDataTinyusb(tUs, getLe32(b), getLe32(b + 4), getLe32(b + 8),
                                      getLe32(b + 12), getLe32(b + 16), tag, kindStr, dir);
            inst_->setReplayChunks(1);
            break;
        }
        case 3: {
            if (bn < 9) break;
            uint8_t raw[13];
            putLe32(raw, kCswSignature);
            memcpy(raw + 4, b, 9);
            inst_->emitBotCswTinyusb(raw, tUs);
            break;
        }
        case 4:
            inst_->emitBotNeedResetTinyusb(tUs);
            break;
        case 5:
            if (bn < 8) break;
            inst_->emitUsbControlTinyusb(b[0], b[1], getLe16(b + 2), getLe16(b + 4),
                                         getLe16(b + 6), tUs, flag != 0);
            break;
        case 6:
            inst_->emitUsbEpochTinyusb(tUs, "set_address", 0, bn >= 2 ? getLe16(b) : 0, nullptr);
            break;
        case 7:
            inst_->emitUsbEpochTinyusb(tUs, "mount", 0, bn >= 2 ? getLe16(b) : 0,
                                       "set_configuration_n_via_tud_mount_cb");
            inst_->emitIdentityDescriptorsAt(tUs, ser ? ser : "HP0001", "HUPROBE", "HU_PROBE",
                                             FW_VERSION);
            inst_->emitPhaseAt(tUs, "enum", "plug");
            break;
        case 8:
            inst_->emitUsbEpochTinyusb(tUs, "umount", 0, 0,
                                       "set_configuration_0_or_bus_via_tud_umount_cb");
            inst_->emitPhaseAt(tUs, "idle", "unplug");
            break;
        case 9:
            inst_->emitUsbEpochTinyusb(tUs, "ep_stall", bn >= 1 ? b[0] : 0, 0, nullptr);
            break;
        case 10:
            inst_->emitUsbEpochTinyusb(tUs, "ep_clear_halt", bn >= 1 ? b[0] : 0, 0,
                                       flag ? "need_reset_active_not_healed" : nullptr);
            break;
        case 11:
            inst_->emitUsbEpochTinyusb(tUs, "msc_bot_reset", 0, 0, "msc_req_reset_0xff");
            break;
        default:
            break;
    }
    EmitOrigin::set("post_finalize", true);
    return true;
}

bool OfflineCapture::drainUpload(uint32_t maxMs) {
    if (!tcp_) return EventBus::instance().pending() == 0;
    unsigned long t0 = millis();
    while (millis() - t0 < maxMs) {
        tcp_->loop();
        yield();
        delay(10);
        if (EventBus::instance().pending() == 0 && tcp_->connected()) return true;
    }
    return EventBus::instance().pending() == 0;
}

void OfflineCapture::restoreUsbAfterUpload() {
    if (!msc_) return;
    if (fallbackDetached_) {
        msc_->softAttach();
        fallbackDetached_ = false;
    }
    if (mediaHidden_) {
        msc_->restoreMedia();
        mediaHidden_ = false;
    }
}

void OfflineCapture::publishOfflineHealth(bool final) {
    if (!session_ || !session_->id().length()) return;
    JsonDocument health;
    int64_t t = ProbeClock::publishUs(ProbeClock::nowUs());
    health["v"] = 2;
    health["sid"] = session_->id();
    health["seq"] = session_->nextSeq();
    health["t_us"] = (long long)t;
    health["t_ms"] = ProbeClock::nowMsCompat(t);
    health["ev"] = "capture_health";
    health["transport"] = "ram_offline";
    health["offline_records"] = recordCount_;
    health["offline_bytes"] = (uint32_t)used_;
    health["offline_overflow"] = overflow_;
    health["offline_post_finalize_records"] = postFinalizeRecords_;
    health["offline_post_finalize_dropped"] = postFinalizeDropped_;
    // B7 fixed (0.2.31): replayed events carry unclamped capture_t_us + capture_seq.
    health["replay_clock"] = "capture_t_us";
    if (inst_) inst_->turStatsJson(health.as<JsonObject>());
    health["offline_wallclock"] = wallclock_;
    if (anchorUnixUs_) {
        health["offline_anchor_unix_us"] = (long long)anchorUnixUs_;
        health["offline_anchor_capture_t_us"] = (long long)anchorCaptureUs_;
    }
    health["offline_time_range_dropped"] = timeRangeDropped_;
    health["replay_records"] = replaySeq_;
    health["replay_complete"] = replayComplete();
    if (replayError_) health["replay_error"] = replayError_;
    health["end_reason"] = stopReason_;
    health["end_strategy"] = endStrategy_;
    health["end_on_unmount"] = endOnUnmount_;
    health["free_markers"] = freeMarkers_;
    health["soft_replugs"] = softReplugs_;
    health["transport_epoch_start"] = transportEpochStart_;
    health["transport_epoch_end"] = transportEpochEnd_;
    health["transport_epoch"] = transportEpochEnd_;
    health["upload_status"] = uploadStatus_;
    const uint32_t dropped = EventBus::instance().dropped();
    const bool drained = EventBus::instance().pending() == 0;
    const bool complete = final && overflow_ == 0 && dropped == 0 && drained &&
                          timeRangeDropped_ == 0 && replayComplete();
    health["dropped"] = dropped;
    health["published"] = EventBus::instance().published();
    health["complete"] = complete;
    health["integrity_pass"] = complete;
    health["record_format"] = kRecFormat;
    const uint32_t recs = recordCount_ + postFinalizeRecords_;
    health["record_size"] = recs ? (uint32_t)((used_ + recs / 2) / recs) : 0;
    health["capacity_bytes"] = (uint32_t)cap_;
    health["arm_heap_free"] = armFreeHeap_;
    health["arm_heap_max_alloc"] = armMaxAlloc_;
    health["heap_free"] = ESP.getFreeHeap();
    health["heap_min_free"] = ESP.getMinFreeHeap();
    health["psram_bytes"] = ESP.getPsramSize();
    health["hw_module"] = HwInfo::moduleGuess();
    PanicTail::appendPrevious(health.as<JsonObject>());
    EventBus::instance().publish(health);
    healthPublished_ = true;
}

void OfflineCapture::beginFinalize(MscGadget* msc) {
    if (state_ != State::Capturing) return;
    // Flush capture-window events while still Capturing; after media_hide the host's
    // reaction is tagged post_finalize.
    MscBotTap::drain(256);
    state_ = State::Finalizing;
    msc_ = msc ? msc : msc_;
    // Quiet USB for WiFi TX via media_hide only. Forced soft-detach here correlated
    // with multi-minute WiFi loss on lab OTG (.88); keep detach as wifiOn failure fallback.
    if (msc_ && msc_->plugged()) {
        if (!mediaHidden_) {
            mediaHidden_ = msc_->hideMedia();
            if (mediaHidden_) PanicTail::breadcrumb(PanicTail::MediaHide);
        }
        if (!mediaHidden_ && !fallbackDetached_ && msc_->usbSoftAttached()) {
            fallbackDetached_ = msc_->softDetach(0);
            if (fallbackDetached_) PanicTail::breadcrumb(PanicTail::SoftDetach);
        }
    }
    transportEpochEnd_ = msc_ ? msc_->transportEpoch() : transportEpochStart_;
    Serial.printf("[OFFLINE] finalize reason=%s records=%u bytes=%u overflow=%u\n",
                  stopReason_.c_str(), recordCount_, (unsigned)used_, overflow_);
    EmitOrigin::set("post_finalize", true);
    state_ = State::Uploading;
    uploadPhase_ = UploadPhase::Wifi;
    uploadPhaseSinceMs_ = millis();
    replayOff_ = 0;
    replaySeq_ = 0;
    replayError_ = nullptr;
    sessionStarted_ = false;
    healthPublished_ = false;
    lastWifiReconnectMs_ = 0;
    uploadStatus_ = "wifi_connecting";
    PanicTail::breadcrumb(PanicTail::WifiUpload, transportEpochEnd_);
}

void OfflineCapture::pollUpload() {
    if (state_ != State::Uploading) return;
    switch (uploadPhase_) {
        case UploadPhase::Wifi: {
            // One blocking wifiOn (≤25s) — only once per capture; App stays responsive after.
            if (WiFi.status() == WL_CONNECTED || wifiOn()) {
                if (tcp_) tcp_->setOfflineDrain(true);
                uploadStatus_ = "wifi_up";
                uploadPhase_ = UploadPhase::Session;
                uploadPhaseSinceMs_ = millis();
            } else {
                // media_hide failed path: one soft-detach then retry WiFi.
                if (mediaHidden_ && !fallbackDetached_ && msc_ && msc_->usbSoftAttached()) {
                    fallbackDetached_ = msc_->softDetach(0);
                    if (fallbackDetached_) PanicTail::breadcrumb(PanicTail::SoftDetach);
                }
                uploadStatus_ = "wifi_retry";
                uploadPhaseSinceMs_ = millis();
            }
            break;
        }
        case UploadPhase::Session: {
            pumpTcp(300);
            if (!tcp_ || !tcp_->connected()) {
                if (millis() - uploadPhaseSinceMs_ > 20000) {
                    uploadStatus_ = "tcp_wait";
                    uploadPhaseSinceMs_ = millis();
                }
                break;
            }
            const char* ser = msc_ ? msc_->identitySerial() : nullptr;
            const char* lab = msc_ ? msc_->volumeLabel() : nullptr;
            if (!session_->start(vehicle_, scenario_, profileId_, ser, lab, "ram_offline",
                                 wallclock_)) {
                uploadStatus_ = "session_failed";
                uploadPhase_ = UploadPhase::Done;
                break;
            }
            sessionStarted_ = true;
            uploadStatus_ = "replaying";
            uploadPhase_ = UploadPhase::Replay;
            uploadPhaseSinceMs_ = millis();
            break;
        }
        case UploadPhase::Replay: {
            // Backpressure: every queued event is a heap String (~300 B, no PSRAM). Replaying
            // while TCP/WiFi is stalled filled the heap until WiFi itself dropped.
            const bool linkUp = WiFi.status() == WL_CONNECTED && tcp_ && tcp_->connected();
            if (!linkUp) {
                if (WiFi.status() != WL_CONNECTED) reconnectWifiThrottled();
                pumpTcp(200);
                if (millis() - uploadPhaseSinceMs_ > kUploadStallMs) {
                    uploadStatus_ = "replay_stalled";
                    uploadPhase_ = UploadPhase::Health;
                    uploadPhaseSinceMs_ = millis();
                }
                break;
            }
            if (EventBus::instance().pending() < kReplayMaxPending) {
                for (int i = 0; i < 10; i++) {
                    if (!replayOneRec()) {
                        uploadPhase_ = UploadPhase::Health;
                        break;
                    }
                }
                uploadPhaseSinceMs_ = millis();
            }
            pumpTcpWhilePending(200, 16);
            break;
        }
        case UploadPhase::Health: {
            // Footer must go out immediately — lab WiFi often dies ~30–60s after wifiOn.
            if (sessionStarted_ && !healthPublished_) {
                uploadStatus_ = "draining";
                publishOfflineHealth(true);
                session_->stop();
                drainUpload(8000);
            }
            uploadPhase_ = UploadPhase::Drain;
            uploadPhaseSinceMs_ = millis();
            break;
        }
        case UploadPhase::Drain: {
            // The timeout must also fire with WiFi down: the App WiFi supervisor is blocked
            // while active(), so this phase is the only way back to Idle.
            const bool timedOut = millis() - uploadPhaseSinceMs_ > kUploadStallMs;
            if (WiFi.status() != WL_CONNECTED) {
                reconnectWifiThrottled();
                if (!timedOut) {
                    pumpTcp(200);
                    break;
                }
            } else {
                drainUpload(400);
            }
            const bool empty = EventBus::instance().pending() == 0 && tcp_ && tcp_->connected();
            if (empty || timedOut) {
                uploadStatus_ = empty ? "delivered" : "queued";
                if (empty) publishOfflineHealth(true);
                const uint32_t dropped = EventBus::instance().dropped();
                saveLastRun(empty && overflow_ == 0 && dropped == 0 && timeRangeDropped_ == 0 &&
                                replayComplete(),
                            dropped);
                if (tcp_) tcp_->setOfflineDrain(false);
                freeBuffer();
                restoreUsbAfterUpload();
                EmitOrigin::clear();
                uploadPhase_ = UploadPhase::Done;
                state_ = State::Idle;
                Serial.printf(
                    "[OFFLINE] upload done status=%s records=%u pending=%u dropped=%u integrity=%d\n",
                    uploadStatus_.c_str(), lastRun_.records,
                    (unsigned)EventBus::instance().pending(), (unsigned)dropped,
                    lastRun_.integrityPass ? 1 : 0);
            }
            break;
        }
        default:
            break;
    }
}

void OfflineCapture::poll(MscGadget* msc) {
    if (state_ == State::Idle) return;

    if (state_ == State::Uploading) {
        pollUpload();
        return;
    }

    // BOOT button (GPIO0): short = marker, long >=1.5s = stop.
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    bool down = digitalRead(RESET_BUTTON_PIN) == LOW;
    if (down && !bootWasDown_) {
        bootDownMs_ = millis();
        bootDownUs_ = ProbeClock::nowUs();
    }
    if (!down && bootWasDown_ && capturing()) {
        unsigned long held = millis() - bootDownMs_;
        if (held >= 1500) {
            pendingTapMs_ = 0;
            requestStop("boot_long");
        } else if (held >= 30) {
            if (!freeMarkers_) {
                // Experiment markers while WiFi is down (audit: marker ohne WiFi).
                queueNextExperimentMarker();
            } else if (pendingTapMs_ && millis() - pendingTapMs_ <= kDoubleTapMs) {
                pendingTapMs_ = 0;
                softReplug();
            } else {
                pendingTapMs_ = millis();
                pendingTapUs_ = bootDownUs_;
            }
        }
    }
    bootWasDown_ = down;
    // Single press confirmed once no second press followed; stamped at the press itself.
    if (pendingTapMs_ && !down && millis() - pendingTapMs_ > kDoubleTapMs) {
        pendingTapMs_ = 0;
        if (capturing()) queueFreeMarker(pendingTapUs_);
    }

    if (capturing()) {
        // Timed end: hide LUN first so USB transport/device-stack evidence remains
        // continuous. Legacy soft detach remains selectable and is also a WiFi fallback.
        if (autoDetachMs_ > 0 && !autoDetached_ &&
            (millis() - captureStartedMs_ >= autoDetachMs_)) {
            autoDetached_ = true;
            MscBotTap::drain(256);
            if (endStrategy_ == "soft_detach") {
                Serial.println("[OFFLINE] auto soft-detach (legacy)");
                fallbackDetached_ = msc && msc->softDetach(0);
                PanicTail::breadcrumb(PanicTail::SoftDetach);
                requestStop("auto_soft_detach");
            } else {
                Serial.println("[OFFLINE] auto media-hide");
                mediaHidden_ = msc && msc->hideMedia();
                PanicTail::breadcrumb(PanicTail::MediaHide);
                if (!mediaHidden_ && msc) {
                    fallbackDetached_ = msc->softDetach(0);
                    if (fallbackDetached_) PanicTail::breadcrumb(PanicTail::SoftDetach);
                }
                requestStop(mediaHidden_ ? "auto_media_hide" : "media_hide_failed");
            }
        }

        bool plugged = msc && msc->plugged();
        if (plugged) {
            wasPlugged_ = true;
            unplugSinceMs_ = 0;
        } else if (endOnUnmount_ && wasPlugged_ && millis() >= ignoreUnplugUntilMs_) {
            if (unplugSinceMs_ == 0) unplugSinceMs_ = millis();
            if (millis() - unplugSinceMs_ >= kUnplugIdleMs) {
                requestStop("unplug_idle");
            }
        }
        if (millis() - captureStartedMs_ >= kMaxCaptureMs) {
            requestStop("max_duration");
        }
        if (stopRequested_) {
            beginFinalize(msc);
        }
    }
}

void OfflineCapture::toBudgetJson(JsonObject obj) const {
    const size_t target = kDefaultCap;
    const size_t fallback = fallbackCapBytes();
    const size_t actual = cap_ ? cap_ : target;
    obj["capacity_bytes"] = (uint32_t)actual;
    obj["capacity_target_bytes"] = (uint32_t)target;
    obj["capacity_fallback_bytes"] = (uint32_t)fallback;
    obj["record_format"] = kRecFormat;
    obj["record_size"] = (uint32_t)kTypicalRecBytes;
    obj["record_size_max"] = (uint32_t)kRecMax;
    obj["estimated_max_records"] = (uint32_t)maxRecordsForCap(actual);
    obj["arm_heap_free"] = armFreeHeap_;
    obj["arm_heap_max_alloc"] = armMaxAlloc_;
    obj["max_duration_ms"] = kMaxCaptureMs;
    obj["measure_ms"] = autoDetachMs_;
    if (capturing() && autoDetachMs_ > 0) {
        unsigned long elapsed = millis() - captureStartedMs_;
        obj["elapsed_ms"] = (uint32_t)elapsed;
        obj["remaining_ms"] =
            (elapsed < autoDetachMs_) ? (uint32_t)(autoDetachMs_ - elapsed) : 0;
    }
}

void OfflineCapture::toStatusJson(JsonObject obj) const {
    obj["state"] = stateName();
    obj["active"] = active();
    obj["capturing"] = capturing();
    obj["records"] = recordCount_;
    obj["post_finalize_records"] = postFinalizeRecords_;
    obj["bytes"] = (uint32_t)used_;
    obj["cap"] = (uint32_t)cap_;
    obj["overflow"] = overflow_;
    obj["wallclock"] = wallclock_;
    obj["stop_reason"] = stopReason_;
    obj["soft_usb"] = softUsb_;
    obj["auto_detach_ms"] = autoDetachMs_;
    obj["end_strategy"] = endStrategy_;
    obj["end_on_unmount"] = endOnUnmount_;
    obj["free_markers"] = freeMarkers_;
    obj["soft_replugs"] = softReplugs_;
    obj["upload_status"] = uploadStatus_;
    obj["media_hidden"] = mediaHidden_;
    obj["fallback_detached"] = fallbackDetached_;
    obj["transport_epoch_start"] = transportEpochStart_;
    obj["transport_epoch_end"] = transportEpochEnd_;
    obj["last_marker"] = lastMarker_;
    obj["marker_index"] = markerIndex_;
    obj["marker_count"] = markerCount_;
    {
        JsonArray arr = obj["markers"].to<JsonArray>();
        for (uint8_t i = 0; i < markerCount_; i++) arr.add(markerSeq_[i]);
    }
    {
        JsonObject budget = obj["capture_budget"].to<JsonObject>();
        toBudgetJson(budget);
    }
    if (lastRun_.valid) {
        JsonObject lr = obj["last_run"].to<JsonObject>();
        lr["wallclock"] = lastRun_.wallclock;
        lr["vehicle"] = lastRun_.vehicle;
        lr["scenario"] = lastRun_.scenario;
        lr["records"] = lastRun_.records;
        lr["bytes"] = lastRun_.bytes;
        lr["cap"] = lastRun_.cap;
        lr["overflow"] = lastRun_.overflow;
        lr["dropped"] = lastRun_.dropped;
        lr["complete"] = lastRun_.complete;
        lr["upload_status"] = lastRun_.uploadStatus;
        lr["stop_reason"] = lastRun_.stopReason;
        lr["end_strategy"] = lastRun_.endStrategy;
        lr["measure_ms"] = lastRun_.measureMs;
        lr["integrity_pass"] = lastRun_.integrityPass;
        lr["integrity"] = lastRun_.integrityPass ? "PASS" : "FAIL";
    }
}
