#include "capture/OfflineCapture.h"
#include "BuildFlags.h"
#include "core/ProbeClock.h"
#include "core/PanicTail.h"
#include "net/EventBus.h"
#include "net/TcpStreamer.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>
#include <cstring>

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
    recordCount_ = 0;
    overflow_ = 0;
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
    Serial.printf("[OFFLINE] pre-alloc free_heap=%u max_alloc=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());

    wallclock_[0] = 0;
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
                                  const char* const* markers, uint8_t markerCount) {
    if (state_ != State::Arming && state_ != State::Idle) return false;
    if (state_ == State::Idle) return false;
    vehicle_ = vehicle;
    scenario_ = scenario;
    profileId_ = profileId;
    softUsb_ = softUsb;
    autoDetachMs_ = autoDetachMs;
    endStrategy_ = (endStrategy == "soft_detach") ? "soft_detach" : "media_hide";
    setMarkerSequence(markers, markerCount);
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

void OfflineCapture::appendMarkerRec(int64_t tUs, const char* name) {
    Rec rec;
    memset(&rec, 0, sizeof(rec));
    rec.kind = kKindMarker;
    rec.tUs = tUs;
    if (name) snprintf(rec.tag, sizeof(rec.tag), "%s", name);
    if (used_ + sizeof(Rec) > cap_) {
        overflow_++;
        return;
    }
    memcpy(buf_ + used_, &rec, sizeof(Rec));
    used_ += sizeof(Rec);
    recordCount_++;
}

bool OfflineCapture::appendPending(uint8_t kind, int64_t tUs, const uint8_t* raw31,
                                   uint32_t botTag, uint32_t lba, uint32_t nBytes,
                                   uint32_t durUs, uint32_t gapUs, uint16_t wValue,
                                   uint16_t wIndex, uint16_t wLength, uint8_t bmRequestType,
                                   uint8_t bRequest, uint8_t epAddr, uint8_t flag,
                                   const char* tag, const char* kindStr, const char* dir) {
    if (!capturing() || !buf_) return false;
    if (used_ + sizeof(Rec) > cap_) {
        overflow_++;
        return false;
    }
    Rec rec;
    memset(&rec, 0, sizeof(rec));
    rec.kind = kind;
    rec.flag = flag;
    rec.bmRequestType = bmRequestType;
    rec.bRequest = bRequest;
    rec.epAddr = epAddr;
    rec.wValue = wValue;
    rec.wIndex = wIndex;
    rec.wLength = wLength;
    rec.tUs = tUs;
    rec.botTag = botTag;
    rec.lba = lba;
    rec.nBytes = nBytes;
    rec.durUs = durUs;
    rec.gapUs = gapUs;
    if (raw31) memcpy(rec.raw, raw31, 31);
    if (tag) snprintf(rec.tag, sizeof(rec.tag), "%s", tag);
    if (kindStr) snprintf(rec.kindStr, sizeof(rec.kindStr), "%s", kindStr);
    if (dir) snprintf(rec.dir, sizeof(rec.dir), "%s", dir);
    memcpy(buf_ + used_, &rec, sizeof(Rec));
    used_ += sizeof(Rec);
    recordCount_++;
    // Host unmount ends the window — but not during soft re-enum grace.
    if (kind == kKindUmount && capturing() && millis() >= ignoreUnplugUntilMs_) {
        requestStop("umount");
    }
    return true;
}

bool OfflineCapture::replayOneRec() {
    if (!inst_ || !buf_ || !session_ || !sessionStarted_) return false;
    if (replayOff_ + sizeof(Rec) > used_) return false;
    Rec rec;
    memcpy(&rec, buf_ + replayOff_, sizeof(Rec));
    replayOff_ += sizeof(Rec);
    const char* ser = msc_ ? msc_->identitySerial() : nullptr;
    if (rec.kind == kKindMarker) {
        session_->marker(rec.tag[0] ? rec.tag : "operator", "offline");
        return true;
    }
    switch (rec.kind) {
        case 1:
            inst_->emitBotCbwTinyusb(rec.raw, rec.tUs);
            break;
        case 2:
            inst_->emitBotDataTinyusb(rec.tUs, rec.botTag, rec.lba, rec.nBytes, rec.durUs,
                                      rec.gapUs, rec.tag, rec.kindStr, rec.dir);
            break;
        case 3:
            inst_->emitBotCswTinyusb(rec.raw, rec.tUs);
            break;
        case 4:
            inst_->emitBotNeedResetTinyusb(rec.tUs);
            break;
        case 5:
            inst_->emitUsbControlTinyusb(rec.bmRequestType, rec.bRequest, rec.wValue, rec.wIndex,
                                         rec.wLength, rec.tUs, rec.flag != 0);
            break;
        case 6:
            inst_->emitUsbEpochTinyusb(rec.tUs, "set_address", 0, rec.wValue, nullptr);
            break;
        case 7:
            inst_->emitUsbEpochTinyusb(rec.tUs, "mount", 0, rec.wValue,
                                       "set_configuration_n_via_tud_mount_cb");
            inst_->emitIdentityDescriptorsAt(rec.tUs, ser ? ser : "HP0001", "HUPROBE", "HU_PROBE",
                                             FW_VERSION);
            inst_->emitPhaseAt(rec.tUs, "enum", "plug");
            break;
        case 8:
            inst_->emitUsbEpochTinyusb(rec.tUs, "umount", 0, 0,
                                       "set_configuration_0_or_bus_via_tud_umount_cb");
            inst_->emitPhaseAt(rec.tUs, "idle", "unplug");
            break;
        case 9:
            inst_->emitUsbEpochTinyusb(rec.tUs, "ep_stall", rec.epAddr, 0, nullptr);
            break;
        case 10:
            inst_->emitUsbEpochTinyusb(rec.tUs, "ep_clear_halt", rec.epAddr, 0,
                                       rec.flag ? "need_reset_active_not_healed" : nullptr);
            break;
        case 11:
            inst_->emitUsbEpochTinyusb(rec.tUs, "msc_bot_reset", 0, 0, "msc_req_reset_0xff");
            break;
        default:
            break;
    }
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
    health["offline_wallclock"] = wallclock_;
    health["end_reason"] = stopReason_;
    health["end_strategy"] = endStrategy_;
    health["transport_epoch_start"] = transportEpochStart_;
    health["transport_epoch_end"] = transportEpochEnd_;
    health["transport_epoch"] = transportEpochEnd_;
    health["upload_status"] = uploadStatus_;
    const uint32_t dropped = EventBus::instance().dropped();
    const bool drained = EventBus::instance().pending() == 0;
    const bool complete = final && overflow_ == 0 && dropped == 0 && drained;
    health["dropped"] = dropped;
    health["published"] = EventBus::instance().published();
    health["complete"] = complete;
    health["integrity_pass"] = complete;
    health["record_size"] = (uint32_t)sizeof(Rec);
    health["capacity_bytes"] = (uint32_t)cap_;
    PanicTail::appendPrevious(health.as<JsonObject>());
    EventBus::instance().publish(health);
    healthPublished_ = true;
}

void OfflineCapture::beginFinalize(MscGadget* msc) {
    if (state_ != State::Capturing) return;
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
    state_ = State::Uploading;
    uploadPhase_ = UploadPhase::Wifi;
    uploadPhaseSinceMs_ = millis();
    replayOff_ = 0;
    sessionStarted_ = false;
    healthPublished_ = false;
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
            // Finish replay before typical post-wifiOn death window (~30–60s on lab OTG).
            for (int i = 0; i < 10; i++) {
                if (!replayOneRec()) {
                    uploadPhase_ = UploadPhase::Health;
                    uploadPhaseSinceMs_ = millis();
                    break;
                }
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
            if (WiFi.status() != WL_CONNECTED) {
                // Short retry slices — do not call full 25s wifiOn every loop.
                WiFi.reconnect();
                pumpTcp(200);
                break;
            }
            drainUpload(400);
            const bool empty = EventBus::instance().pending() == 0 && tcp_ && tcp_->connected();
            const bool timedOut = millis() - uploadPhaseSinceMs_ > 120000;
            if (empty || timedOut) {
                uploadStatus_ = empty ? "delivered" : "queued";
                if (empty) publishOfflineHealth(true);
                const uint32_t dropped = EventBus::instance().dropped();
                saveLastRun(empty && overflow_ == 0 && dropped == 0, dropped);
                if (tcp_) tcp_->setOfflineDrain(false);
                freeBuffer();
                restoreUsbAfterUpload();
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
    }
    if (!down && bootWasDown_ && capturing()) {
        unsigned long held = millis() - bootDownMs_;
        if (held >= 1500) {
            requestStop("boot_long");
        } else if (held >= 30) {
            // Experiment markers while WiFi is down (audit: marker ohne WiFi).
            queueNextExperimentMarker();
        }
    }
    bootWasDown_ = down;

    if (capturing()) {
        // Timed end: hide LUN first so USB transport/device-stack evidence remains
        // continuous. Legacy soft detach remains selectable and is also a WiFi fallback.
        if (autoDetachMs_ > 0 && !autoDetached_ &&
            (millis() - captureStartedMs_ >= autoDetachMs_)) {
            autoDetached_ = true;
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
        } else if (wasPlugged_ && millis() >= ignoreUnplugUntilMs_) {
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
    obj["record_size"] = (uint32_t)sizeof(Rec);
    obj["estimated_max_records"] = (uint32_t)maxRecordsForCap(actual);
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
    obj["bytes"] = (uint32_t)used_;
    obj["cap"] = (uint32_t)cap_;
    obj["overflow"] = overflow_;
    obj["wallclock"] = wallclock_;
    obj["stop_reason"] = stopReason_;
    obj["soft_usb"] = softUsb_;
    obj["auto_detach_ms"] = autoDetachMs_;
    obj["end_strategy"] = endStrategy_;
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
