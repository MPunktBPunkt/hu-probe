#include "App.h"
#include "BuildFlags.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_system.h>

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
    doc["chipModel"] = HUPROBE_BOARD_LABEL;
    doc["hwType"] = HUPROBE_HW_TYPE;

    JsonObject ios = doc["ios"].to<JsonObject>();
    ios["probe_state"] = a.session_.active() ? a.session_.phase() : "idle";
    ios["session_id"] = a.session_.id();
    ios["read_rate_kb_s"] = a.instrument_.readRateKbS();
    ios["tur_hz"] = a.instrument_.turHz();
    ios["scenario"] = a.config_.scenarioId;
    ios["vehicle"] = a.config_.vehicleId;
}

void App::setupWifi() {
    WiFiManager wm;
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setHostname(config_.deviceName.c_str());
    if (!wm.autoConnect(WIFI_AP_NAME)) {
        Serial.println("[WIFI] portal timeout — restart");
        delay(1000);
        ESP.restart();
    }
    Serial.printf("[WIFI] %s\n", WiFi.localIP().toString().c_str());
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

    config_.begin();
    EventBus::instance().begin();
    session_.begin();
    instrument_.begin(&session_);
    msc_.begin();

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
}

void App::emitStatus() {
    if (!session_.active()) return;
    JsonDocument doc;
    doc["v"] = 1;
    doc["sid"] = session_.id();
    doc["seq"] = session_.nextSeq();
    doc["t_ms"] = (int)millis();
    doc["ev"] = "status";
    doc["read_rate_kb_s"] = instrument_.readRateKbS();
    doc["tur_hz"] = instrument_.turHz();
    doc["probe_state"] = session_.phase();
    doc["scenario"] = session_.scenario();
    doc["vehicle"] = session_.vehicle();
    EventBus::instance().publish(doc);
}

void App::loop() {
    hub_.loop();
    tcp_.loop();
    web_.loop();
    msc_.loop();
    instrument_.tick();

    unsigned long now = millis();
    if (now - lastStatusMs_ >= STATUS_INTERVAL_MS) {
        lastStatusMs_ = now;
        emitStatus();
    }
}
