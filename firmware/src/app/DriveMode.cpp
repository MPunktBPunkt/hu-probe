/**
 * @file DriveMode.cpp
 * @brief Drive mode boot phase (WLAN, trip upload, OTA, arm), host-wait and capture state handling.
 */

#include "app/DriveMode.h"
#include "capture/ExperimentProfiles.h"
#include "capture/OfflineCapture.h"
#include "capture/TripStore.h"
#include "core/ConfigStore.h"
#include "core/DiagLog.h"
#include "core/FieldPlan.h"
#include "core/GithubOta.h"
#include "media/ProbeMedia.h"
#include "msc/ReadStall.h"
#include "msc/MscGadget.h"
#include "net/TcpStreamer.h"
#include "session/Session.h"
#include "ui/StatusLed.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>
#include <time.h>

DriveMode& DriveMode::instance() {
    static DriveMode d;
    return d;
}

const char* DriveMode::stateName() const {
    switch (state_) {
        case State::Off: return "off";
        case State::Boot: return "boot";
        case State::WaitHost: return "wait_host";
        case State::Capturing: return "capturing";
        case State::CarIdle: return "car_idle";
        case State::Normal: return "normal";
    }
    return "?";
}

void DriveMode::begin(ConfigStore* config, Session* session, MscGadget* msc,
                      ReadInstrument* inst, TcpStreamer* tcp, StatusLed* led) {
    config_ = config;
    session_ = session;
    msc_ = msc;
    inst_ = inst;
    tcp_ = tcp;
    led_ = led;
    state_ = State::Boot;
    led_->showDrivePhase(StatusLed::DrivePhase::Check);
    // WiFiManager keeps the home WLAN in the esp_wifi NVS config; read it before any
    // WiFi.begin(ssid, pass) replaces the RAM copy.
    WiFi.mode(WIFI_STA);
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK && conf.sta.ssid[0]) {
        homeSsid_ = String(reinterpret_cast<const char*>(conf.sta.ssid));
        homePass_ = String(reinterpret_cast<const char*>(conf.sta.password));
    }
    TripStore::instance().begin();
    DiagLog::printf("[DRIVE] boot home=%s hotspot=%s trips_pending=%u\n",
                    homeSsid_.length() ? homeSsid_.c_str() : "-",
                    config_->wifi2Ssid.length() ? config_->wifi2Ssid.c_str() : "-",
                    (unsigned)TripStore::instance().pendingCount());
}

bool DriveMode::hasCredentials() const {
    return homeSsid_.length() || config_->wifi2Ssid.length();
}

bool DriveMode::connectThunk(uint32_t timeoutMs) { return instance().connectWifi(timeoutMs); }

bool DriveMode::connectWifi(uint32_t timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) return true;
    if (!hasCredentials()) return false;
    // persistent(false): the hotspot must never overwrite the home WLAN in NVS.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    esp_wifi_start();
    WiFi.setAutoReconnect(false);
    WiFiMulti multi;
    if (homeSsid_.length()) multi.addAP(homeSsid_.c_str(), homePass_.c_str());
    if (config_->wifi2Ssid.length())
        multi.addAP(config_->wifi2Ssid.c_str(), config_->wifi2Pass.c_str());
    const unsigned long t0 = millis();
    uint8_t st = multi.run(timeoutMs);
    while (st != WL_CONNECTED && millis() - t0 < timeoutMs) {
        delay(250);
        st = WiFi.status();
    }
    WiFi.persistent(true);
    if (st != WL_CONNECTED) {
        DiagLog::printf("[DRIVE] no WLAN after %lu ms\n", (unsigned long)(millis() - t0));
        return false;
    }
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setAutoReconnect(false);
    lastSsid_ = WiFi.SSID();
    DiagLog::printf("[DRIVE] WLAN %s %s in %lu ms\n", lastSsid_.c_str(),
                    WiFi.localIP().toString().c_str(), (unsigned long)(millis() - t0));
    return true;
}

bool DriveMode::collectorReachable() const {
    if (!config_->enableCollector || WiFi.status() != WL_CONNECTED) return false;
    WiFiClient c;
    const bool ok = c.connect(config_->collectorHost.c_str(), (uint16_t)config_->collectorPort, 1500);
    c.stop();
    return ok;
}

void DriveMode::uploadPending() {
    TripStore& ts = TripStore::instance();
    OfflineCapture& oc = OfflineCapture::instance();
    for (uint8_t i = 0; i < 8; i++) {
        const TripStore::Trip* t = ts.firstPending();
        if (!t) break;
        const uint32_t id = t->meta.tripId;
        if (!oc.replayStored(session_, msc_, inst_, tcp_)) break;
        const unsigned long t0 = millis();
        while (oc.active() && millis() - t0 < kTripUploadMaxMs) {
            oc.poll(msc_);
            tcp_->loop();
            delay(2);
        }
        const TripStore::Trip* again = ts.firstPending();
        if (again && again->meta.tripId == id) {
            lastError_ = "trip_upload_incomplete";
            break;
        }
        uploaded_++;
    }
}

bool DriveMode::onHomeWifi() const {
    if (WiFi.status() != WL_CONNECTED || !homeSsid_.length()) return false;
    // Hotspot alone must not count as "home" — otherwise a phone AP in the car would
    // clear autoCapture and skip the next trip.
    if (config_->wifi2Ssid.length() && WiFi.SSID() == config_->wifi2Ssid &&
        WiFi.SSID() != homeSsid_) {
        return false;
    }
    return WiFi.SSID() == homeSsid_;
}

void DriveMode::disableAutoCapture(const char* why) {
    if (!config_->autoCapture) return;
    config_->autoCapture = false;
    config_->save();
    DiagLog::printf("[DRIVE] autoCapture cleared (%s)\n", why ? why : "?");
}

void DriveMode::bootPhase() {
    DiagLog::persist("drive", true);
    TripStore& ts = TripStore::instance();
    // Accept the image before the (possibly long) upload: pulling the plug during an upload
    // must not count as a failed boot and trigger a rollback.
    GithubOta::markHealthy();
    const bool wifi = WiFi.status() == WL_CONNECTED;
    const bool home = onHomeWifi();
    if (wifi && config_->enableNtp) {
        const unsigned long t0 = millis();
        while (time(nullptr) < 1600000000 && millis() - t0 < 3000) delay(100);
    }
    if (wifi && collectorReachable()) {
        collectorSeen_ = true;
        if (ts.pendingCount()) {
            led_->showDrivePhase(StatusLed::DrivePhase::Upload);
            uploadPending();
        }
        ts.resetIfDone();
    }
    if (wifi) FieldPlan::refresh(config_->fieldPlanUrl, nullptr);
    const bool plan = FieldPlan::active();
    // Without a field plan: one-shot Heimfahrt — at home with collector, upload then clear the
    // flag so the next boot is normal probe mode. Hotspot / no WLAN → keep measuring (arm below).
    if (home) {
        if (plan) {
            DiagLog::printf("[DRIVE] home, field plan active — keep autoCapture\n");
        } else if (collectorSeen_ && ts.pendingCount() == 0) {
            disableAutoCapture("home_collector_done");
        } else if (collectorSeen_) {
            DiagLog::printf("[DRIVE] home but %u trip(s) still pending — keep autoCapture\n",
                            (unsigned)ts.pendingCount());
        } else {
            DiagLog::printf("[DRIVE] home WLAN, collector unreachable — keep autoCapture\n");
        }
    }
    if (wifi && config_->autoUpdate) {
        led_->showDrivePhase(StatusLed::DrivePhase::Ota);
        GithubOta::check(config_->otaManifestUrl, &otaInfo_);  // restarts on update
    }
    led_->showDrivePhase(StatusLed::DrivePhase::Check);
    if (FieldPlan::armBlocked()) {
        DiagLog::printf("[DRIVE] field plan online_only, not fetched this boot — no capture\n");
        if (wifi) {
            toNormal();
            return;
        }
        esp_wifi_stop();
        msc_->softAttach();
        state_ = State::CarIdle;
        return;
    }
    // Home WLAN normally means desk/charger; arm_at_home records the outbound trip when the
    // car is parked within reach of it. No host mount within kHostWaitMs drops the run.
    if (home && !FieldPlan::armAtHome()) {
        toNormal();
        return;
    }
    if (home) DiagLog::printf("[DRIVE] home WLAN, field plan arm_at_home — arming\n");
    ts.preErase(kPreEraseBytes, kPreEraseMaxMs);
    armCapture();
}

void DriveMode::armCapture() {
    TripStore& ts = TripStore::instance();
    OfflineCapture& oc = OfflineCapture::instance();
    if (ts.available() && !ts.canStart()) {
        lastError_ = "trips_full";
        led_->setTripsFull(true);
        DiagLog::printf("[DRIVE] trip journal full (%u pending) — no capture\n",
                        (unsigned)ts.pendingCount());
        msc_->softAttach();
        state_ = WiFi.status() == WL_CONNECTED ? State::Normal : State::CarIdle;
        return;
    }
    const huprobe::PlanJob* job = FieldPlan::currentJob();
    const ExperimentProfile* exp =
        findExperiment(job ? job->experiment : config_->autoExperiment.c_str());
    if (!exp) exp = findExperiment("drive_home");
    if (!exp) exp = findExperiment("free");
    if (!exp) {
        lastError_ = "no_experiment";
        toNormal();
        return;
    }
    const String media = (job && job->media[0]) ? String(job->media) : config_->autoMedia;
    if (media.length() && strcmp(ProbeMedia::instance().variantId(), media.c_str()) != 0) {
        String err;
        if (!msc_->switchMedia(media.c_str(), nullptr, false, &err, true)) {
            DiagLog::printf("[DRIVE] media %s failed: %s\n", media.c_str(), err.c_str());
        }
    }
    oc.setWifiConnector(&DriveMode::connectThunk);
    oc.setPersist(true, ProbeMedia::instance().variantId());
    if (!oc.arm(session_, msc_, inst_, tcp_)) {
        lastError_ = "arm_failed";
        oc.setPersist(false);
        msc_->softAttach();
        state_ = State::Normal;
        return;
    }
    uint32_t measure = (job && job->measureS) ? job->measureS * 1000UL : exp->measureMs;
    if (measure > OfflineCapture::maxCaptureMs()) measure = OfflineCapture::maxCaptureMs();
    char planScenario[32];
    planRun_ = job && FieldPlan::currentScenario(planScenario, sizeof(planScenario));
    const String scenario = planRun_ ? String(planScenario) : String(exp->scenario);
    const bool fine = job ? job->fine : config_->autoFine;
    // softUsb=true: MSC is detached, so the re-enum is the HU's first sight of the stick.
    if (!oc.beginCapture(config_->vehicleId, scenario, config_->profileId, true, measure,
                         exp->endStrategy, exp->markers, exp->markerCount,
                         !exp->keepOnUnmount, exp->freeMarkers, fine)) {
        lastError_ = "begin_failed";
        planRun_ = false;
        oc.requestStop("begin_failed");
        state_ = State::Capturing;
        return;
    }
    const bool stall = job && job->stall[0];
    if (stall) {
        // Configured after the media switch: a file filter resolves against the new medium.
        JsonDocument sd;
        String err;
        if (deserializeJson(sd, job->stall)) err = "json";
        if (err.length() || !ReadStall::instance().configure(sd.as<JsonVariantConst>(), &err)) {
            lastError_ = String("stall_config:") + err;
            planRun_ = false;
            DiagLog::printf("[DRIVE] plan stall rejected (%s) — run not counted\n", err.c_str());
        }
    }
    led_->setDriveCapture(true);
    hostDeadlineMs_ = millis() + kHostWaitMs;
    state_ = State::WaitHost;
    DiagLog::printf("[DRIVE] capture armed exp=%s scenario=%s media=%s fine=%d stall=%d trip=%u\n",
                    exp->id, scenario.c_str(), ProbeMedia::instance().variantId(), fine ? 1 : 0,
                    stall && ReadStall::instance().enabled() ? 1 : 0,
                    (unsigned)TripStore::instance().currentTripId());
}

void DriveMode::toNormal() {
    connectWifi(kWifiBootMs);
    if (!msc_->usbSoftAttached()) msc_->softAttach();
    session_->start(config_->vehicleId, config_->scenarioId, config_->profileId,
                    msc_->identitySerial(), msc_->volumeLabel());
    state_ = State::Normal;
    DiagLog::printf("[DRIVE] normal mode wifi=%d\n", WiFi.status() == WL_CONNECTED ? 1 : 0);
}

void DriveMode::poll() {
    OfflineCapture& oc = OfflineCapture::instance();
    switch (state_) {
        case State::WaitHost:
            if (msc_->plugged()) {
                state_ = State::Capturing;
                DiagLog::printf("[DRIVE] host mounted — trip running\n");
                if (planRun_) FieldPlan::advance();
                planRun_ = false;
            } else if (!oc.capturing()) {
                state_ = State::Capturing;
            } else if ((long)(millis() - hostDeadlineMs_) >= 0) {
                planRun_ = false;
                oc.abortNoHost();
                led_->setDriveCapture(false);
                toNormal();
            }
            break;
        case State::Capturing:
            if (!oc.active()) {
                led_->setDriveCapture(false);
                if (WiFi.status() == WL_CONNECTED) {
                    toNormal();
                } else {
                    // No WLAN in the car: keep the radio off instead of coex/restart cycles.
                    esp_wifi_stop();
                    state_ = State::CarIdle;
                    DiagLog::printf("[DRIVE] trip done, no WLAN — car idle\n");
                }
            }
            break;
        default:
            break;
    }
}

bool DriveMode::ownsWifi() const {
    return state_ == State::Boot || state_ == State::WaitHost || state_ == State::Capturing ||
           state_ == State::CarIdle;
}

void DriveMode::toJson(JsonObject obj) const {
    obj["enabled"] = enabled();
    obj["state"] = stateName();
    obj["wlan"] = lastSsid_;
    obj["home_ssid"] = homeSsid_;
    obj["on_home_wifi"] = onHomeWifi();
    obj["collector_seen"] = collectorSeen_;
    obj["trips_uploaded"] = uploaded_;
    obj["auto_capture"] = config_ ? config_->autoCapture : false;
    if (otaInfo_.length()) obj["ota"] = otaInfo_;
    FieldPlan::toJson(obj["field_plan"].to<JsonObject>());
    if (lastError_.length()) obj["error"] = lastError_;
    const TripStore& ts = TripStore::instance();
    obj["trips_pending"] = ts.pendingCount();
    obj["trips_free_bytes"] = (uint32_t)ts.freeBytes();
}
