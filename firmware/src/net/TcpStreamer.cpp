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
    // Peek → write → pop. Never drop a seq on failed TX (root cause of silent gaps).
    // Cap + yield: a 192-event flush under MSC load starved WiFi/HTTP (lab WiFi-death).
    String line;
    int n = 0;
    EventBus& bus = EventBus::instance();
    while (n < 48) {
        if (!client_.connected()) {
            wasConnected_ = false;
            bus.noteTcpDisconnect();
            return;
        }
        if (!bus.peek(line)) return;
        size_t need = line.length() + 1;  // + '\n'
        size_t w = client_.print(line);
        w += client_.print('\n');
        if (w < need) {
            bus.noteTcpTxFail();
            // Leave line at head; retry next loop / after reconnect.
            return;
        }
        String discarded;
        if (!bus.pop(discarded)) {
            return;
        }
        bus.noteTcpTxOk();
        n++;
        if ((n & 7) == 0) yield();
    }
}
