/**
 * @file App.cpp
 * @brief App boot sequence, main loop, status/health publishing and WiFi/USB coex supervision.
 */

#include "App.h"
#include "app/DriveMode.h"
#include "capture/TripStore.h"
#include "core/DiagLog.h"
#include "core/GithubOta.h"
#include "BuildFlags.h"
#include "BuildId.h"
#include "capture/AutoMarker.h"
#include "capture/OfflineCapture.h"
#include "capture/SequenceRunner.h"
#include "core/HwInfo.h"
#include "core/ProbeClock.h"
#include "core/PanicTail.h"
#include "core/PhaseGate.h"
#include "msc/MscBotTap.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef CAPTURE_HEALTH_INTERVAL_MS
#define CAPTURE_HEALTH_INTERVAL_MS 5000
#endif
#ifndef NVS_TAIL_INTERVAL_MS
#define NVS_TAIL_INTERVAL_MS 60000
#endif
#ifndef WIFI_RECONNECT_EVERY_MS
#define WIFI_RECONNECT_EVERY_MS 10000
#endif
#ifndef WIFI_RESTART_AFTER_MS
#define WIFI_RESTART_AFTER_MS 25000
#endif

/** WiFi assoc up but no TCP event progress while queue backs up → treat as link dead. */
#ifndef WIFI_TCP_STALL_MS
#define WIFI_TCP_STALL_MS 8000
#endif
#ifndef WIFI_TCP_STALL_PENDING
#define WIFI_TCP_STALL_PENDING 32
#endif

/** After WiFi returns from coex soft-detach: hold USB off until ring drains. */
#ifndef WIFI_COEX_FLUSH_MS
#define WIFI_COEX_FLUSH_MS 15000
#endif

/** Only full-chip restart if soft-detach still cannot bring WiFi back. */
#ifndef WIFI_RESTART_AFTER_COEX_MS
#define WIFI_RESTART_AFTER_COEX_MS 90000
#endif

/** Reattach only when EventBus is this quiet (was: timer alone → kill WiFi again). */
#ifndef WIFI_COEX_REATTACH_PENDING
#define WIFI_COEX_REATTACH_PENDING 16
#endif

/** No stall until TCP has succeeded at least once + this grace after STA up. */
#ifndef WIFI_STALL_GRACE_MS
#define WIFI_STALL_GRACE_MS 30000
#endif

/** tcp_tx_ok within this window ⇒ IP data path alive. */
#ifndef WIFI_TCP_ALIVE_MS
#define WIFI_TCP_ALIVE_MS 8000
#endif

/** Zombie (assoc, no data) before hard STA restart in coex hold. */
#ifndef WIFI_COEX_ZOMBIE_RESTART_MS
#define WIFI_COEX_ZOMBIE_RESTART_MS 20000
#endif

App& App::instance() {
    static App app;
    return app;
}

void App::buildHeartbeat(JsonDocument& doc) {
    App& a = App::instance();
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    doc["mac"] = macStr;
    doc["ip"] = WiFi.localIP().toString();
    doc["name"] = a.config_.deviceName;
    doc["fwType"] = FW_TYPE;
    doc["fwVersion"] = FW_VERSION;
    doc["fwSha"] = FW_GIT_SHA;
    doc["fwBuildId"] = FW_BUILD_ID;
    doc["chipModel"] = HUPROBE_BOARD_LABEL;
    doc["hwType"] = HUPROBE_HW_TYPE;
    doc["hwModule"] = HwInfo::moduleGuess();

    JsonObject ios = doc["ios"].to<JsonObject>();
    ios["probe_state"] = a.session_.active() ? a.session_.phase() : "idle";
    ios["session_id"] = a.session_.id();
    ios["read_rate_kb_s"] = a.instrument_.readRateKbS();
    ios["tur_hz"] = a.instrument_.turHz();
    ios["scenario"] = a.config_.scenarioId;
    ios["vehicle"] = a.config_.vehicleId;
    ios["dropped"] = EventBus::instance().dropped();
}

void App::setupWifi(bool forcePortal) {
    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setHostname(config_.deviceName.c_str());
    led_.setWifiPortal(true);
    const bool ok = forcePortal ? wm.startConfigPortal(WIFI_AP_NAME) : wm.autoConnect(WIFI_AP_NAME);
    if (!ok && forcePortal) {
        // Drive mode: an abandoned portal must not cost the trip.
        DiagLog::printf("[WIFI] portal closed without connection\n");
        led_.setWifiPortal(false);
        return;
    }
    if (!ok) {
        DiagLog::printf("[WIFI] portal timeout — restart");
        PanicTail::noteRestart(PanicTail::RestartPortalTimeout);
        delay(1000);
        ESP.restart();
    }
    led_.setWifiPortal(false);
    // USB-MSC + WiFi: modem sleep + low TX were Lab OTG killers (yellow within seconds).
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    // Manual superviseWifi owns reconnect — auto + manual race under coex.
    WiFi.setAutoReconnect(false);
    wifiDownSinceMs_ = 0;
    wifiReconnectAttempts_ = 0;
    wifiConnectedAtMs_ = millis();
    DiagLog::printf("[WIFI] %s ps=none tx=19.5 autoReconnect=0\n",
                  WiFi.localIP().toString().c_str());
}

void App::setupMdns() {
    if (!config_.enableMdns) return;
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char host[32];
    snprintf(host, sizeof(host), "huprobe-%02x%02x%02x", mac[3], mac[4], mac[5]);
    if (MDNS.begin(host)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("[mDNS] %s.local\n", host);
    }
}

void App::begin() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[hu-probe] " FW_VERSION);

    PanicTail::begin();
    DiagLog::begin();
    PhaseGate::begin();
    GithubOta::bootCheck();
    HwInfo::begin();
    led_.begin();
    config_.begin();
    EventBus::instance().begin();
    session_.begin();
    instrument_.begin(&session_);
    AutoMarker::begin(&session_);
    TripStore::instance().begin();
    const bool drive = config_.autoCapture;
    // MSC first so the HU sees a stick even while WiFi portal runs. Drive mode keeps it
    // detached until the capture is armed, so the HU's first enumeration is recorded.
    msc_.begin(&session_, &instrument_, drive);

    if (drive) {
        pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
        const bool forcePortal = digitalRead(RESET_BUTTON_PIN) == LOW;
        if (forcePortal) setupWifi(true);
        DriveMode& dm = DriveMode::instance();
        dm.begin(&config_, &session_, &msc_, &instrument_, &tcp_, &led_);
        if (WiFi.status() != WL_CONNECTED) {
            if (dm.hasCredentials()) dm.connectWifi(15000);
            else setupWifi(true);
        }
    } else {
        setupWifi();
    }
    if (WiFi.status() == WL_CONNECTED) setupMdns();

    // Lab: keep MSC soft-detached by default — Enum/Mount must not kill WiFi.
    // Reattach only via WebUI / Sequence "MSC arm" (explicit softAttach).
    if (!drive && isLabDebian()) {
        msc_.softDetach(0);
        labMscDetachedDefault_ = true;
        DiagLog::printf("[MSC] lab_debian: soft-detached by default (WiFi first)");
    }

    if (config_.enableNtp) {
        configTime(0, 0, config_.ntpServer.c_str());
        setenv("TZ", config_.tz.c_str(), 1);
        tzset();
    }

    hub_.begin(&config_);
    hub_.setPayloadBuilder(buildHeartbeat);
    tcp_.begin(&config_);
    web_.begin(&config_, &session_, &instrument_, &msc_, &hub_, &tcp_);
    SequenceRunner::instance().begin(&session_, &msc_, &instrument_, &tcp_);
    if (WiFi.status() == WL_CONNECTED) hub_.sendNow();

    if (drive) {
        DriveMode::instance().bootPhase();
    } else {
        GithubOta::markHealthy();
        // Auto-session for field use: plug → measure without opening the UI first.
        session_.start(config_.vehicleId, config_.scenarioId, config_.profileId,
                       msc_.identitySerial(), msc_.volumeLabel());
        Serial.printf("[SESSION] auto-start %s fw=%s sha=%s\n", session_.id().c_str(),
                      FW_VERSION, FW_GIT_SHA);
    }

    // Above TinyUSB usbd (prio 24): Arduino loop was starved under OTG, coex never ran.
    xTaskCreatePinnedToCore(wifiCoexTask, "wifiCoex", 4096, this, 25, &wifiCoexTaskHandle_, 0);
    DiagLog::printf("[WIFI] coex task prio=25 core=0");
}

void App::wifiCoexTask(void* arg) {
    App* self = static_cast<App*>(arg);
    for (;;) {
        // Ownership (0.3.9): TcpStreamer/WiFiClient must not be touched from this task while
        // stored-trip replay or the upload batch runs — OfflineCapture/WebUi pump TCP from
        // the Arduino loop only. Parallel tcp_.loop() + enterCoexHold (esp_wifi_stop) raced
        // WiFiClient::write and matched the trip_replay hangs (see feldbericht 2026-10-21).
        const bool tripUp = self->web_.tripUploadActive();
        const bool ocBlocks = OfflineCapture::instance().blocksWifiSupervise();
        if (!ocBlocks && !DriveMode::instance().ownsWifi() && !tripUp) {
            self->superviseWifi();
        }
        // Do NOT call tcp_.loop() here. Main App::loop() (+ OfflineCapture pumps during
        // replay) is the sole TcpStreamer owner. Coex soft-detach frees the loop from USB
        // bulk starvation, so a second flusher is unnecessary and unsafe.
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void App::emitStatus() {
    // Entire offline arm→upload owns the session/TCP path — no live status spam.
    if (OfflineCapture::instance().active()) return;
    if (!session_.active()) return;
    int64_t t = ProbeClock::publishUs(ProbeClock::eventNowUs());
    JsonDocument doc;
    doc["v"] = 2;
    doc["sid"] = session_.id();
    doc["seq"] = session_.nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "status";
    doc["read_rate_kb_s"] = instrument_.readRateKbS();
    doc["tur_hz"] = instrument_.turHz();
    doc["probe_state"] = session_.phase();
    doc["scenario"] = session_.scenario();
    doc["vehicle"] = session_.vehicle();
    EventBus::instance().publish(doc);
}

void App::emitCaptureHealth() {
    if (OfflineCapture::instance().active()) return;
    if (!session_.active()) return;
    int64_t t = ProbeClock::publishUs(ProbeClock::eventNowUs());
    EventBus& bus = EventBus::instance();
    JsonDocument doc;
    doc["v"] = 2;
    doc["sid"] = session_.id();
    doc["seq"] = session_.nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "capture_health";
    bus.fillCaptureHealth(doc.as<JsonObject>(), session_.seqFirst(), session_.seq(),
                          tcp_.connected());
    doc["msc_sample_overflow"] = msc_.sampleOverflow();
    doc["bot_pending_overflow"] = MscBotTap::pendingOverflow();
    doc["msc_reads"] = msc_.readCount();
    doc["transport"] = session_.transport();
    doc["transport_epoch"] = msc_.transportEpoch();
    doc["end_reason"] = session_.active() ? "active" : "session_stop";
    doc["upload_status"] = tcp_.connected() ? "streaming" : "pending";
    instrument_.turStatsJson(doc.as<JsonObject>());
    PanicTail::appendPrevious(doc.as<JsonObject>());
    DiagLog::appendPreviousTail(doc.as<JsonObject>(), session_.id().c_str());
    doc["fw_sha"] = FW_GIT_SHA;
    doc["fw_build_id"] = FW_BUILD_ID;
    doc["fw_dirty"] = FW_GIT_DIRTY != 0;
    doc["hw_module"] = HwInfo::moduleGuess();
    bus.publish(doc);

    // NVS every ~60s only — flash erase during MSC+WiFi was a field WiFi-killer.
    unsigned long now = millis();
    if (now - lastNvsTailMs_ >= NVS_TAIL_INTERVAL_MS) {
        lastNvsTailMs_ = now;
        Preferences prefs;
        if (prefs.begin("huprobe", false)) {
            prefs.putUInt("last_seq", session_.seq());
            prefs.putString("last_sid", session_.id());
            prefs.end();
        }
    }
}

bool App::isLabDebian() const {
    return config_.vehicleId == "lab_debian" || config_.profileId == "lab_debian";
}

bool App::tcpDataAlive(unsigned long now) const {
    if (!config_.enableCollector) return true;
    const unsigned long lastOk = EventBus::instance().lastTcpTxOkMs();
    if (lastOk == 0) return false;
    return (now - lastOk) < WIFI_TCP_ALIVE_MS;
}

bool App::linkAlive(unsigned long now) const {
    if (tcpDataAlive(now)) return true;
    return EventBus::instance().pending() == 0;
}

void App::enterCoexHold(const char* reason) {
    EventBus& bus = EventBus::instance();
    const size_t pendingBefore = bus.pending();
    PanicTail::CoexReason code = PanicTail::CoexOther;
    if (reason) {
        if (strcmp(reason, "assoc-lost") == 0) code = PanicTail::CoexAssocLost;
        else if (strcmp(reason, "stall+usb") == 0) code = PanicTail::CoexStallUsb;
        else if (strcmp(reason, "stall+soft") == 0) code = PanicTail::CoexStallSoft;
        else if (strcmp(reason, "zombie-assoc") == 0) code = PanicTail::CoexZombie;
    }
    PanicTail::breadcrumb(PanicTail::CoexHold, code);
    if (msc_.usbSoftAttached()) {
        msc_.softDetach(0);
    }
    coexDetachedForWifi_ = true;
    coexRecoveredAtMs_ = 0;
    coexZombieSinceMs_ = 0;
    // B23: never wipe live session evidence — only clear when idle / no session.
    // Trip-upload drain also needs the ring; clearing here made "drain ok" after data loss.
    if (!session_.active() && !web_.tripUploadActive()) {
        bus.clear();
    } else {
        DiagLog::printf("[WIFI] coex-hold keep ring pending=%u (session=%d trip_up=%d)\n",
                        (unsigned)pendingBefore, session_.active() ? 1 : 0,
                        web_.tripUploadActive() ? 1 : 0);
    }
    WiFi.setAutoReconnect(false);
    // Soft reconnect() is not enough after OTG RF death — full MAC stop/start.
    WiFi.disconnect(false, false);
    delay(20);
    esp_wifi_stop();
    delay(200);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.begin();  // NVS creds (WiFiManager)
    WiFi.setSleep(false);
    if (wifiDownSinceMs_ == 0) wifiDownSinceMs_ = millis();
    lastWifiReconnectMs_ = millis();
    wifiReconnectAttempts_++;
    DiagLog::printf(
        "[WIFI] coex-hold (%s) pending_was=%u heap=%u psram_free=%u soft=%d\n",
        reason ? reason : "?", (unsigned)pendingBefore, (unsigned)ESP.getFreeHeap(),
        (unsigned)ESP.getFreePsram(), msc_.usbSoftAttached() ? 1 : 0);
    DiagLog::persist("coex", false);
}

void App::superviseWifi() {
    unsigned long now = millis();
    EventBus& bus = EventBus::instance();
    bool assoc = (WiFi.status() == WL_CONNECTED);
    const bool dataAlive = tcpDataAlive(now);
    const bool alive = linkAlive(now);
    const bool sockOk = tcp_.connected();

    // Stall only after real TX progress — lastOk==0 must NOT mean "idle since boot".
    const size_t stallPending =
        msc_.plugged() ? (size_t)16 : (size_t)WIFI_TCP_STALL_PENDING;
    const unsigned long stallMs = msc_.plugged() ? 2500UL : (unsigned long)WIFI_TCP_STALL_MS;
    if (assoc && config_.enableCollector && bus.pending() >= stallPending) {
        const unsigned long lastOk = bus.lastTcpTxOkMs();
        const bool tcpEverWorked = (lastOk != 0);
        const bool pastGrace =
            (wifiConnectedAtMs_ != 0) && (now - wifiConnectedAtMs_ >= WIFI_STALL_GRACE_MS);
        const bool tcpStalled = tcpEverWorked && (now - lastOk >= stallMs);
        if (tcpStalled && pastGrace &&
            now - lastWifiReconnectMs_ >= WIFI_RECONNECT_EVERY_MS) {
            lastWifiReconnectMs_ = now;
            DiagLog::printf(
                "[WIFI] tcp-stall pending=%u idle=%lums lastOk=%lu sock=%d heap=%u plugged=%d\n",
                (unsigned)bus.pending(), (unsigned long)(now - lastOk), lastOk, sockOk ? 1 : 0,
                (unsigned)ESP.getFreeHeap(), msc_.plugged() ? 1 : 0);
            enterCoexHold(msc_.usbSoftAttached() ? "stall+usb" : "stall+soft");
            return;
        }
    }

    if (assoc) {
        if (wifiConnectedAtMs_ == 0) wifiConnectedAtMs_ = now;

        // Real recovery = recent collector TX, not WL_CONNECTED / socket alone.
        const bool linkOk =
            !coexDetachedForWifi_ || alive || !config_.enableCollector;

        if (linkOk) {
            if (wifiDownSinceMs_ != 0) {
                DiagLog::printf(
                    "[WIFI] recovered after %lu ms (attempts=%u coex=%d heap=%u "
                    "data=%d sock=%d)\n",
                    (unsigned long)(now - wifiDownSinceMs_), wifiReconnectAttempts_,
                    coexDetachedForWifi_ ? 1 : 0, (unsigned)ESP.getFreeHeap(), dataAlive ? 1 : 0,
                    sockOk ? 1 : 0);
            }
            // Watchdog reset only on real data progress (not tcp_.connected() flicker).
            if (alive || !config_.enableCollector) {
                wifiDownSinceMs_ = 0;
                wifiReconnectAttempts_ = 0;
                coexZombieSinceMs_ = 0;
            }
            if (coexDetachedForWifi_ && alive && coexRecoveredAtMs_ == 0)
                coexRecoveredAtMs_ = now;
        } else {
            if (wifiDownSinceMs_ == 0) {
                wifiDownSinceMs_ = now;
                DiagLog::printf(
                    "[WIFI] zombie assoc (coex hold, no data) heap=%u sock=%d pending=%u\n",
                    (unsigned)ESP.getFreeHeap(), sockOk ? 1 : 0, (unsigned)bus.pending());
            }
            if (coexZombieSinceMs_ == 0) coexZombieSinceMs_ = now;
        }

        if (coexDetachedForWifi_) {
            unsigned long flushedFor =
                (coexRecoveredAtMs_ == 0) ? 0 : (now - coexRecoveredAtMs_);
            unsigned long zombieFor =
                (coexZombieSinceMs_ == 0) ? 0 : (now - coexZombieSinceMs_);

            // Own zombie clock — independent of coexRecoveredAtMs_ / tcpOk.
            if (!alive && zombieFor >= WIFI_COEX_ZOMBIE_RESTART_MS &&
                now - lastWifiReconnectMs_ >= WIFI_RECONNECT_EVERY_MS) {
                lastWifiReconnectMs_ = now;
                DiagLog::printf("[WIFI] coex zombie — STA restart zombie=%lums\n", zombieFor);
                PanicTail::breadcrumb(PanicTail::ZombieRestart, (uint32_t)zombieFor);
                enterCoexHold("zombie-assoc");
                return;
            }

            bool ringQuiet = bus.pending() < WIFI_COEX_REATTACH_PENDING;
            // Lab default-detach: never auto-reattach (needs explicit MSC arm).
            const bool mayReattach = !labMscDetachedDefault_;
            if (mayReattach && ringQuiet && alive && flushedFor >= WIFI_COEX_FLUSH_MS) {
                DiagLog::printf("[WIFI] coex reattach pending=%u hold=%lums heap=%u\n",
                              (unsigned)bus.pending(), flushedFor, (unsigned)ESP.getFreeHeap());
                PanicTail::breadcrumb(PanicTail::CoexReattach, (uint32_t)flushedFor);
                msc_.softAttach();
                coexDetachedForWifi_ = false;
                coexRecoveredAtMs_ = 0;
                coexZombieSinceMs_ = 0;
            } else if (now - lastWifiReconnectMs_ >= WIFI_RECONNECT_EVERY_MS) {
                lastWifiReconnectMs_ = now;
                DiagLog::printf(
                    "[WIFI] coex waiting pending=%u data=%d sock=%d hold=%lums zombie=%lums "
                    "labDetach=%d\n",
                    (unsigned)bus.pending(), dataAlive ? 1 : 0, sockOk ? 1 : 0, flushedFor,
                    zombieFor, labMscDetachedDefault_ ? 1 : 0);
            }
        }
        if (linkOk) return;
        // Fall through while zombie assoc (watchdog / reconnect).
    } else {
        wifiConnectedAtMs_ = 0;
        coexZombieSinceMs_ = 0;
    }

    if (wifiDownSinceMs_ == 0) {
        wifiDownSinceMs_ = now;
        DiagLog::printf("[WIFI] lost — supervising reconnect heap=%u\n",
                      (unsigned)ESP.getFreeHeap());
        PanicTail::breadcrumb(PanicTail::WifiLost, assoc ? 1u : 0u);
        if (msc_.usbSoftAttached() || !coexDetachedForWifi_) {
            enterCoexHold("assoc-lost");
        }
    }
    unsigned long downFor = now - wifiDownSinceMs_;
    if (now - lastWifiReconnectMs_ >= WIFI_RECONNECT_EVERY_MS) {
        lastWifiReconnectMs_ = now;
        wifiReconnectAttempts_++;
        DiagLog::printf("[WIFI] reconnect #%u (down %lu ms coex=%d heap=%u data=%d)\n",
                      wifiReconnectAttempts_, downFor, coexDetachedForWifi_ ? 1 : 0,
                      (unsigned)ESP.getFreeHeap(), dataAlive ? 1 : 0);
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        delay(30);
        WiFi.begin();
    }
    unsigned long restartAfter =
        coexDetachedForWifi_ ? WIFI_RESTART_AFTER_COEX_MS : WIFI_RESTART_AFTER_MS;
    if (downFor >= restartAfter && wifiReconnectAttempts_ >= 3) {
        if (web_.tripUploadActive()) {
            // Keep soft-reconnecting; a hard restart aborts the batch and loses RAM state.
            DiagLog::printf("[WIFI] unrecoverable deferred (trip upload) pending=%u\n",
                            (unsigned)bus.pending());
            DiagLog::persist("trip_upload", false);
            wifiReconnectAttempts_ = 0;
            wifiDownSinceMs_ = now;
            return;
        }
        DiagLog::printf("[WIFI] unrecoverable — ESP.restart()");
        PanicTail::noteRestart(PanicTail::RestartWifiUnrecoverable);
        delay(200);
        ESP.restart();
    }
}

void App::loop() {
    // WiFi/coex soft-detach runs in wifiCoexTask (loop starves under USB Bulk).
    // Drain Capture-tUs BOT/read before loop-now status/health (keeps t_us non-decreasing).
    msc_.loop();
    OfflineCapture::instance().poll(&msc_);
    DriveMode::instance().poll();
    SequenceRunner::instance().poll();
    instrument_.tick();
    tcp_.loop();
    web_.loop();

    const bool usbHost = msc_.plugged();
    // Hub HTTPClient can block up to 2s — under OTG that races WiFi to death.
    if (!usbHost) {
        hub_.loop();
    } else if (millis() - lastHubWhileUsbMs_ >= 60000UL) {
        lastHubWhileUsbMs_ = millis();
        hub_.loop();
    }

    // lab_debian never reattaches after a hold; detached is its normal state there.
    led_.setCoexHold(coexDetachedForWifi_ && !labMscDetachedDefault_);
    led_.setTripUpload(web_.tripUploadActive() ||
                       OfflineCapture::instance().replayingStored());
    led_.tick(session_, msc_, instrument_, tcp_, EventBus::instance());

    unsigned long now = millis();
    // During coex hold: no status/health publish — refill was keeping the stall latch set.
    if (!coexDetachedForWifi_) {
        unsigned long statusEvery = usbHost ? (STATUS_INTERVAL_MS * 2) : STATUS_INTERVAL_MS;
        unsigned long healthEvery =
            usbHost ? (CAPTURE_HEALTH_INTERVAL_MS * 2) : CAPTURE_HEALTH_INTERVAL_MS;
        if (now - lastStatusMs_ >= statusEvery) {
            lastStatusMs_ = now;
            emitStatus();
        }
        if (now - lastCaptureHealthMs_ >= healthEvery) {
            lastCaptureHealthMs_ = now;
            emitCaptureHealth();
        }
    }
    if (usbHost) yield();
}
