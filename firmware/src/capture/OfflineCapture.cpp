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
    cap_ = kDefaultCap;
    buf_ = static_cast<uint8_t*>(malloc(cap_));
    if (!buf_) {
        cap_ = 48 * 1024;
        buf_ = static_cast<uint8_t*>(malloc(cap_));
    }
    if (!buf_) {
        cap_ = 0;
        return false;
    }
    used_ = 0;
    recordCount_ = 0;
    overflow_ = 0;
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
        delay(20);
        if (EventBus::instance().pending() == 0 && tcp_->connected()) break;
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
    delay(100);

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

bool OfflineCapture::beginCapture(const String& vehicle, const String& scenario,
                                  const String& profileId, bool softUsb,
                                  uint32_t autoDetachMs, const String& endStrategy) {
    if (state_ != State::Arming && state_ != State::Idle) return false;
    if (state_ == State::Idle) return false;
    vehicle_ = vehicle;
    scenario_ = scenario;
    profileId_ = profileId;
    softUsb_ = softUsb;
    autoDetachMs_ = autoDetachMs;
    endStrategy_ = (endStrategy == "soft_detach") ? "soft_detach" : "media_hide";
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
    appendMarkerRec(ProbeClock::nowUs(), name ? name : "operator");
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

bool OfflineCapture::replayAll() {
    if (!inst_ || !buf_ || !session_) return false;
    // Fresh session with transport tag — publishes start to EventBus (WiFi up).
    const char* ser = msc_ ? msc_->identitySerial() : nullptr;
    const char* lab = msc_ ? msc_->volumeLabel() : nullptr;
    if (!session_->start(vehicle_, scenario_, profileId_, ser, lab, "ram_offline", wallclock_)) {
        return false;
    }

    size_t off = 0;
    while (off + sizeof(Rec) <= used_) {
        Rec rec;
        memcpy(&rec, buf_ + off, sizeof(Rec));
        off += sizeof(Rec);
        if (rec.kind == kKindMarker) {
            session_->marker(rec.tag[0] ? rec.tag : "operator", "offline");
            continue;
        }
        switch (rec.kind) {
            case 1:  // Cbw
                inst_->emitBotCbwTinyusb(rec.raw, rec.tUs);
                break;
            case 2:  // Data
                inst_->emitBotDataTinyusb(rec.tUs, rec.botTag, rec.lba, rec.nBytes, rec.durUs,
                                          rec.gapUs, rec.tag, rec.kindStr, rec.dir);
                break;
            case 3:  // Csw
                inst_->emitBotCswTinyusb(rec.raw, rec.tUs);
                break;
            case 4:
                inst_->emitBotNeedResetTinyusb(rec.tUs);
                break;
            case 5:
                inst_->emitUsbControlTinyusb(rec.bmRequestType, rec.bRequest, rec.wValue,
                                             rec.wIndex, rec.wLength, rec.tUs, rec.flag != 0);
                break;
            case 6:
                inst_->emitUsbEpochTinyusb(rec.tUs, "set_address", 0, rec.wValue, nullptr);
                break;
            case 7:
                inst_->emitUsbEpochTinyusb(rec.tUs, "mount", 0, rec.wValue,
                                           "set_configuration_n_via_tud_mount_cb");
                inst_->emitIdentityDescriptorsAt(rec.tUs, ser ? ser : "HP0001", "HUPROBE",
                                                 "HU_PROBE", FW_VERSION);
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
                inst_->emitUsbEpochTinyusb(
                    rec.tUs, "ep_clear_halt", rec.epAddr, 0,
                    rec.flag ? "need_reset_active_not_healed" : nullptr);
                break;
            case 11:
                inst_->emitUsbEpochTinyusb(rec.tUs, "msc_bot_reset", 0, 0, "msc_req_reset_0xff");
                break;
            default:
                break;
        }
        if ((off / sizeof(Rec) & 7) == 7) yield();
    }

    // Footer health note as capture_health-ish marker fields via session stop note.
    JsonDocument foot;
    int64_t t = ProbeClock::publishUs(ProbeClock::nowUs());
    foot["v"] = 2;
    foot["sid"] = session_->id();
    foot["seq"] = session_->nextSeq();
    foot["t_us"] = (long long)t;
    foot["t_ms"] = ProbeClock::nowMsCompat(t);
    foot["ev"] = "capture_health";
    foot["transport"] = "ram_offline";
    foot["offline_records"] = recordCount_;
    foot["offline_bytes"] = (uint32_t)used_;
    foot["offline_overflow"] = overflow_;
    foot["offline_wallclock"] = wallclock_;
    foot["offline_stop"] = stopReason_;
    foot["end_reason"] = stopReason_;
    foot["end_strategy"] = endStrategy_;
    foot["transport_epoch_start"] = transportEpochStart_;
    foot["transport_epoch_end"] = transportEpochEnd_;
    foot["transport_epoch"] = transportEpochEnd_;
    foot["upload_status"] = uploadStatus_;
    PanicTail::appendPrevious(foot.as<JsonObject>());
    foot["dropped"] = EventBus::instance().dropped();
    foot["published"] = EventBus::instance().published();
    foot["complete"] = (overflow_ == 0);
    EventBus::instance().publish(foot);

    session_->stop();
    return true;
}

bool OfflineCapture::finalizeAndUpload() {
    if (state_ != State::Capturing && state_ != State::Finalizing &&
        state_ != State::Uploading) {
        return false;
    }
    if (state_ == State::Capturing) state_ = State::Finalizing;
    if (msc_ && msc_->plugged()) {
        if (endStrategy_ == "media_hide" && !mediaHidden_) {
            mediaHidden_ = msc_->hideMedia();
            PanicTail::breadcrumb(PanicTail::MediaHide);
            if (!mediaHidden_) {
                fallbackDetached_ = msc_->softDetach(0);
                if (fallbackDetached_) PanicTail::breadcrumb(PanicTail::SoftDetach);
            }
        } else if (endStrategy_ == "soft_detach" && !fallbackDetached_ &&
                   msc_->usbSoftAttached()) {
            fallbackDetached_ = msc_->softDetach(0);
            if (fallbackDetached_) PanicTail::breadcrumb(PanicTail::SoftDetach);
        }
    }
    transportEpochEnd_ = msc_ ? msc_->transportEpoch() : transportEpochStart_;
    Serial.printf("[OFFLINE] finalize reason=%s records=%u bytes=%u overflow=%u\n",
                  stopReason_.c_str(), recordCount_, (unsigned)used_, overflow_);

    state_ = State::Uploading;
    uploadStatus_ = "wifi_connecting";
    PanicTail::breadcrumb(PanicTail::WifiUpload, transportEpochEnd_);
    bool okWifi = wifiOn();
    if (!okWifi) {
        uploadStatus_ = "wifi_retry";
        // Media-hide is the non-disruptive first choice. Keep the old detach path
        // as recovery when coexistence still prevents WiFi from returning.
        if (mediaHidden_ && !fallbackDetached_ && msc_) {
            fallbackDetached_ = msc_->softDetach(0);
            if (fallbackDetached_) PanicTail::breadcrumb(PanicTail::SoftDetach);
        }
        Serial.println("[OFFLINE] WiFi failed — will retry in poll()");
        return false;
    }
    // Must establish TCP before/during replay — App::loop is blocked here.
    pumpTcp(8000);
    uploadStatus_ = "replay_enqueued";
    bool okReplay = replayAll();
    pumpTcp(20000);
    uploadStatus_ = okReplay
                        ? ((EventBus::instance().pending() == 0) ? "delivered" : "queued")
                        : "replay_failed";
    if (session_ && session_->id().length()) {
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
        health["end_reason"] = stopReason_;
        health["end_strategy"] = endStrategy_;
        health["transport_epoch_start"] = transportEpochStart_;
        health["transport_epoch_end"] = transportEpochEnd_;
        health["transport_epoch"] = transportEpochEnd_;
        health["upload_status"] = uploadStatus_;
        health["dropped"] = EventBus::instance().dropped();
        health["published"] = EventBus::instance().published();
        health["complete"] = okReplay && overflow_ == 0;
        PanicTail::appendPrevious(health.as<JsonObject>());
        EventBus::instance().publish(health);
        pumpTcp(5000);
    }
    Serial.printf("[OFFLINE] done replay=%d records=%u pending=%u tcp=%d\n",
                  okReplay ? 1 : 0, recordCount_, (unsigned)EventBus::instance().pending(),
                  tcp_ && tcp_->connected() ? 1 : 0);
    freeBuffer();
    if (msc_) {
        if (fallbackDetached_) msc_->softAttach();
        if (mediaHidden_) msc_->restoreMedia();
    }
    state_ = State::Idle;
    return okReplay;
}

void OfflineCapture::poll(MscGadget* msc) {
    if (state_ == State::Idle) return;

    // Retry upload if WiFi failed after capture end.
    if (state_ == State::Uploading && buf_) {
        static unsigned long lastRetryMs = 0;
        if (millis() - lastRetryMs >= 3000) {
            lastRetryMs = millis();
            finalizeAndUpload();
        }
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
            queueMarker("boot");
            Serial.println("[OFFLINE] marker boot");
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
            finalizeAndUpload();
        }
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
}
