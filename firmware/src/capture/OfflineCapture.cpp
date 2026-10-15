#include "capture/OfflineCapture.h"
#include "BuildFlags.h"
#include "core/ProbeClock.h"
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
    Serial.println("[OFFLINE] WiFi OFF");
    WiFi.disconnect(true, false);
    delay(20);
    WiFi.mode(WIFI_OFF);
    delay(50);
    return true;
}

bool OfflineCapture::wifiOn() {
    Serial.println("[OFFLINE] WiFi ON (STA reconnect)");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setAutoReconnect(true);
    WiFi.begin();  // NVS creds from WiFiManager
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
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

bool OfflineCapture::arm(Session* session, MscGadget* msc, ReadInstrument* inst) {
    if (active()) return false;
    session_ = session;
    msc_ = msc;
    inst_ = inst;
    state_ = State::Arming;
    stopRequested_ = false;
    stopReason_ = "";
    wasPlugged_ = false;
    unplugSinceMs_ = 0;

    if (session_ && session_->active()) session_->stop();
    // Drain any live TCP backlog before killing WiFi.
    for (int i = 0; i < 40; i++) {
        // TcpStreamer not passed — EventBus will flush when WiFi returns.
        delay(50);
        yield();
    }

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
        return false;
    }
    if (!wifiOff()) {
        freeBuffer();
        state_ = State::Idle;
        return false;
    }
    // Settling so assoc teardown does not overlap MSC.
    delay(500);
    Serial.printf("[OFFLINE] armed wallclock=%s buf=%u\n", wallclock_, (unsigned)cap_);
    return true;
}

bool OfflineCapture::beginCapture(const String& vehicle, const String& scenario,
                                  const String& profileId) {
    if (state_ != State::Arming && state_ != State::Idle) return false;
    if (state_ == State::Idle) return false;
    vehicle_ = vehicle;
    scenario_ = scenario;
    profileId_ = profileId;
    captureStartedMs_ = millis();
    wasPlugged_ = msc_ && msc_->plugged();
    state_ = State::Capturing;
    Serial.printf("[OFFLINE] capturing vehicle=%s scenario=%s\n", vehicle_.c_str(),
                  scenario_.c_str());
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
    foot["dropped"] = EventBus::instance().dropped();
    foot["published"] = EventBus::instance().published();
    foot["complete"] = (overflow_ == 0);
    EventBus::instance().publish(foot);

    session_->stop();
    return true;
}

bool OfflineCapture::finalizeAndUpload() {
    if (state_ != State::Capturing && state_ != State::Finalizing) return false;
    state_ = State::Finalizing;
    Serial.printf("[OFFLINE] finalize reason=%s records=%u bytes=%u overflow=%u\n",
                  stopReason_.c_str(), recordCount_, (unsigned)used_, overflow_);

    state_ = State::Uploading;
    bool okWifi = wifiOn();
    if (!okWifi) {
        Serial.println("[OFFLINE] keeping RAM buffer — WiFi failed (retry via /api/offline/upload)");
        // Stay in Uploading with buffer retained so a later retry can work.
        return false;
    }
    bool okReplay = replayAll();
    freeBuffer();
    state_ = State::Idle;
    Serial.printf("[OFFLINE] done replay=%d pending=%u (TCP drains in App::loop)\n",
                  okReplay ? 1 : 0, (unsigned)EventBus::instance().pending());
    return okReplay;
}

void OfflineCapture::poll(MscGadget* msc) {
    if (state_ == State::Idle) return;

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
        bool plugged = msc && msc->plugged();
        if (plugged) {
            wasPlugged_ = true;
            unplugSinceMs_ = 0;
        } else if (wasPlugged_) {
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
}
