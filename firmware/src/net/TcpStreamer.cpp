#include "TcpStreamer.h"
#include "EventBus.h"
#include <WiFi.h>

void TcpStreamer::begin(ConfigStore* config) {
    config_ = config;
    lastConnectAttempt_ = 0;
    wasConnected_ = false;
}

void TcpStreamer::loop() {
    if (!config_ || !config_->enableCollector) return;
    if (WiFi.status() != WL_CONNECTED) return;

    bool now = client_.connected();
    if (wasConnected_ && !now) {
        EventBus::instance().noteTcpDisconnect();
        Serial.println("[TCP] disconnected");
    }
    wasConnected_ = now;

    if (!now) {
        tryConnect();
        return;
    }
    flushPending();
}

void TcpStreamer::tryConnect() {
    unsigned long now = millis();
    if (now - lastConnectAttempt_ < 3000) return;
    lastConnectAttempt_ = now;
    Serial.printf("[TCP] connect %s:%d\n",
                  config_->collectorHost.c_str(), config_->collectorPort);
    if (client_.connect(config_->collectorHost.c_str(), config_->collectorPort)) {
        Serial.println("[TCP] connected");
        EventBus::instance().noteTcpConnect();
        wasConnected_ = true;
        flushPending();
    }
}

void TcpStreamer::flushPending() {
    String line;
    int n = 0;
    while (n < 64 && EventBus::instance().pop(line)) {
        client_.print(line);
        client_.print('\n');
        n++;
    }
}
