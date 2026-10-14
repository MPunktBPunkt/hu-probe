#include "App.h"
#include "BuildFlags.h"
#include "BuildId.h"
#include "core/ProbeClock.h"
#include "msc/MscBotTap.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <Preferences.h>

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

    JsonObject ios = doc["ios"].to<JsonObject>();
    ios["probe_state"] = a.session_.active() ? a.session_.phase() : "idle";
    ios["session_id"] = a.session_.id();
    ios["read_rate_kb_s"] = a.instrument_.readRateKbS();
    ios["tur_hz"] = a.instrument_.turHz();
    ios["scenario"] = a.config_.scenarioId;
    ios["vehicle"] = a.config_.vehicleId;
    ios["dropped"] = EventBus::instance().dropped();
}

void App::setupWifi() {
    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setHostname(config_.deviceName.c_str());
    led_.setWifiPortal(true);
    if (!wm.autoConnect(WIFI_AP_NAME)) {
        Serial.println("[WIFI] portal timeout — restart");
        delay(1000);
        ESP.restart();
    }
    led_.setWifiPortal(false);
    // USB-MSC + WiFi: modem sleep + low TX were Lab OTG killers (yellow within seconds).
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setAutoReconnect(true);
    wifiDownSinceMs_ = 0;
    wifiReconnectAttempts_ = 0;
    Serial.printf("[WIFI] %s ps=none tx=19.5\n", WiFi.localIP().toString().c_str());
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

    led_.begin();
    config_.begin();
    EventBus::instance().begin();
    session_.begin();
    instrument_.begin(&session_);
    // MSC first so the HU sees a stick even while WiFi portal runs.
    msc_.begin(&session_, &instrument_);

    setupWifi();
    setupMdns();

    if (config_.enableNtp) {
        configTime(0, 0, config_.ntpServer.c_str());
        setenv("TZ", config_.tz.c_str(), 1);
        tzset();
    }

    hub_.begin(&config_);
    hub_.setPayloadBuilder(buildHeartbeat);
    tcp_.begin(&config_);
    web_.begin(&config_, &session_, &instrument_, &msc_, &hub_, &tcp_);
    hub_.sendNow();

    // Auto-session for field use: plug → measure without opening the UI first.
    session_.start(config_.vehicleId, config_.scenarioId, config_.profileId,
                   msc_.identitySerial(), msc_.volumeLabel());
    Serial.printf("[SESSION] auto-start %s fw=%s sha=%s\n", session_.id().c_str(), FW_VERSION,
                  FW_GIT_SHA);
}

void App::emitStatus() {
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
    doc["event_seq_first"] = session_.seqFirst();
    doc["event_seq_last"] = session_.seq();
    doc["dropped"] = bus.dropped();
    doc["published"] = bus.published();
    doc["stream_lost"] = bus.streamLost();
    doc["tcp_tx_ok"] = bus.tcpTxOk();
    doc["tcp_tx_fail"] = bus.tcpTxFail();
    doc["queue_pending"] = (uint32_t)bus.pending();
    doc["queue_highwater"] = bus.highwater();
    doc["tcp_connected"] = tcp_.connected();
    doc["tcp_disconnects"] = bus.tcpDisconnects();
    doc["ring_size"] = EVENT_RING_SIZE;
    doc["msc_sample_overflow"] = msc_.sampleOverflow();
    doc["bot_pending_overflow"] = MscBotTap::pendingOverflow();
    doc["msc_reads"] = msc_.readCount();
    doc["fw_sha"] = FW_GIT_SHA;
    doc["fw_build_id"] = FW_BUILD_ID;
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

void App::superviseWifi() {
    unsigned long now = millis();
    EventBus& bus = EventBus::instance();
    bool assoc = (WiFi.status() == WL_CONNECTED);

    // TCP backlog with assoc up: log only — WiFi.disconnect() here wedged lab recovery.
    if (assoc && config_.enableCollector && bus.pending() >= WIFI_TCP_STALL_PENDING) {
        unsigned long lastOk = bus.lastTcpTxOkMs();
        unsigned long idle = (lastOk == 0) ? now : (now - lastOk);
        if (idle >= WIFI_TCP_STALL_MS && now - lastWifiReconnectMs_ >= WIFI_RECONNECT_EVERY_MS) {
            lastWifiReconnectMs_ = now;
            Serial.printf("[WIFI] tcp-stall pending=%u idle=%lums (no assoc bounce)\n",
                          (unsigned)bus.pending(), idle);
        }
    }

    if (assoc) {
        if (wifiDownSinceMs_ != 0) {
            Serial.printf("[WIFI] recovered after %lu ms (attempts=%u)\n",
                          (unsigned long)(now - wifiDownSinceMs_), wifiReconnectAttempts_);
        }
        wifiDownSinceMs_ = 0;
        wifiReconnectAttempts_ = 0;
        return;
    }

    if (wifiDownSinceMs_ == 0) {
        wifiDownSinceMs_ = now;
        Serial.println("[WIFI] lost — supervising reconnect");
    }
    unsigned long downFor = now - wifiDownSinceMs_;
    if (now - lastWifiReconnectMs_ >= WIFI_RECONNECT_EVERY_MS) {
        lastWifiReconnectMs_ = now;
        wifiReconnectAttempts_++;
        Serial.printf("[WIFI] reconnect #%u (down %lu ms)\n", wifiReconnectAttempts_, downFor);
        WiFi.disconnect(false, false);
        delay(50);
        WiFi.reconnect();
    }
    if (downFor >= WIFI_RESTART_AFTER_MS && wifiReconnectAttempts_ >= 3) {
        Serial.println("[WIFI] unrecoverable — ESP.restart()");
        delay(200);
        ESP.restart();
    }
}

void App::loop() {
    superviseWifi();
    // Drain Capture-tUs BOT/read before loop-now status/health (keeps t_us non-decreasing).
    msc_.loop();
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

    led_.tick(session_, msc_, instrument_, tcp_, EventBus::instance());

    unsigned long now = millis();
    unsigned long statusEvery = usbHost ? (STATUS_INTERVAL_MS * 2) : STATUS_INTERVAL_MS;
    unsigned long healthEvery = usbHost ? (CAPTURE_HEALTH_INTERVAL_MS * 2) : CAPTURE_HEALTH_INTERVAL_MS;
    if (now - lastStatusMs_ >= statusEvery) {
        lastStatusMs_ = now;
        emitStatus();
    }
    if (now - lastCaptureHealthMs_ >= healthEvery) {
        lastCaptureHealthMs_ = now;
        emitCaptureHealth();
    }
    if (usbHost) yield();
}
