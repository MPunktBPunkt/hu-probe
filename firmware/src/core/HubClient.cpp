#include "HubClient.h"
#include "capture/OfflineCapture.h"
#include "core/PanicTail.h"
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>

void HubClient::begin(ConfigStore* config) {
    config_ = config;
    lastSuccess_ = millis();
    lastHeartbeat_ = 0;
}

void HubClient::setPayloadBuilder(void (*builder)(JsonDocument& doc)) {
    builder_ = builder;
}

void HubClient::sendNow() {
    sendHeartbeat();
    lastHeartbeat_ = millis();
}

int HubClient::postJson(const char* path, const String& payload, uint16_t timeoutMs) {
    if (!config_ || !path || WiFi.status() != WL_CONNECTED) return -1;
    String url = "http://" + config_->hubHost + ":" + String(config_->hubPort) + path;
    HTTPClient http;
    http.begin(url);
    http.addHeader(F("Content-Type"), F("application/json"));
    http.setTimeout(timeoutMs);
    int code = http.POST(payload);
    if (code == 200) lastSuccess_ = millis();
    http.end();
    return code;
}

void HubClient::loop() {
    if (!config_ || !config_->enableHub) return;
    // Never OTA / heartbeat-drive resets while offline capture owns RAM + WiFi.
    // Hub may still advertise PiDrive otaUrl for this MAC → would ESP.restart()
    // and wipe the RAM capture before TCP replay finishes.
    if (OfflineCapture::instance().blocksWifiSupervise()) {
        otaPending_ = false;
        otaUrl_ = "";
        return;
    }
    unsigned long now = millis();
    unsigned long interval = (unsigned long)config_->heartbeatIntervalS * 1000UL;
    if (interval < 5000UL) interval = 5000UL;
    if (now - lastHeartbeat_ >= interval) {
        lastHeartbeat_ = now;
        sendHeartbeat();
    }
    if (otaPending_) {
        otaPending_ = false;
        performOta(otaUrl_);
        otaUrl_ = "";
    }
}

void HubClient::sendHeartbeat() {
    if (!config_) return;
    if (WiFi.status() != WL_CONNECTED) {
        lastOk_ = false;
        return;
    }
    JsonDocument doc;
    if (builder_) builder_(doc);
    String payload;
    serializeJson(doc, payload);
    String url = "http://" + config_->hubHost + ":" + String(config_->hubPort) + "/api/register";
    HTTPClient http;
    http.begin(url);
    http.addHeader(F("Content-Type"), F("application/json"));
    http.setTimeout(2000);  // keep loop responsive under MSC load (was 8s WiFi stall risk)
    int code = http.POST(payload);
    lastOk_ = (code == 200);
    if (code == 200) {
        String body = http.getString();
        lastSuccess_ = millis();
        JsonDocument resp;
        if (deserializeJson(resp, body) == DeserializationError::Ok) {
            if (!resp["interval"].isNull()) {
                unsigned long ni = (unsigned long)resp["interval"].as<int>();
                if (ni >= 5 && ni <= 600) config_->heartbeatIntervalS = (uint16_t)ni;
            }
            if (!resp["otaUrl"].isNull()) {
                String u = resp["otaUrl"].as<String>();
                // Cross-flash guard: hub still lists this MAC as PiDrive — refuse
                // non-hu-probe artifacts so a heartbeat cannot wipe a field session.
                if (u.length() > 0) {
                    String ul = u;
                    ul.toLowerCase();
                    if (ul.indexOf("hu-probe") < 0 &&
                        (ul.indexOf("pidrive") >= 0 || ul.indexOf("pi-drive") >= 0)) {
                        Serial.printf("[OTA] ignore foreign url %s\n", u.c_str());
                    } else if (OfflineCapture::instance().blocksWifiSupervise()) {
                        Serial.println("[OTA] ignore during offline capture");
                    } else {
                        otaPending_ = true;
                        otaUrl_ = u;
                    }
                }
            }
        }
    }
    http.end();
}

void HubClient::performOta(const String& url) {
    Serial.println("[OTA] " + url);
    HTTPClient http;
    http.begin(url);
    http.setTimeout(30000);
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[OTA] Fehler: %d\n", code);
        http.end();
        return;
    }
    int len = http.getSize();
    WiFiClient* stream = http.getStreamPtr();
    if (!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)) {
        Serial.println("[OTA] Update.begin fehlgeschlagen");
        http.end();
        return;
    }
    size_t written = Update.writeStream(*stream);
    if (written == 0 || !Update.end()) {
        Serial.println("[OTA] Schreiben fehlgeschlagen");
        http.end();
        return;
    }
    http.end();
    Serial.println("[OTA] OK — Reboot");
    PanicTail::noteRestart(PanicTail::RestartHubOta);
    delay(500);
    ESP.restart();
}
