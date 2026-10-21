/**
 * @file TcpStreamer.cpp
 * @brief Collector connect/reconnect handling and loss-free batched JSONL flushing.
 */

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

void TcpStreamer::forceReconnect() {
    if (client_.connected()) {
        client_.stop();
    }
    if (wasConnected_) {
        EventBus::instance().noteTcpDisconnect();
        wasConnected_ = false;
    }
    lastConnectAttempt_ = 0;  // next loop() may tryConnect immediately
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
    //
    // Critical: one atomic write of (line + '\n'). A split print(line)/print('\n') that
    // partially succeeds used to retry the whole line → collector "Extra data" / seq gaps.
    String line;
    int n = 0;
    const int batch = offlineDrain_ ? 8 : 32;
    EventBus& bus = EventBus::instance();
    while (n < batch) {
        if (!client_.connected()) {
            wasConnected_ = false;
            bus.noteTcpDisconnect();
            return;
        }
        if (!bus.peek(line)) return;
        const size_t need = line.length() + 1;  // + '\n'
        // Build contiguous packet so WiFiClient cannot interleave mid-record.
        String packet;
        packet.reserve(need);
        packet += line;
        packet += '\n';
        size_t w = client_.write(reinterpret_cast<const uint8_t*>(packet.c_str()), need);
        if (w != need) {
            bus.noteTcpTxFail();
            // Partial bytes may already be at the collector — resync with a clean reconnect.
            client_.stop();
            wasConnected_ = false;
            bus.noteTcpDisconnect();
            return;  // leave line at head for retry after reconnect
        }
        if (offlineDrain_) {
            client_.flush();
        }
        String discarded;
        if (!bus.pop(discarded)) {
            return;
        }
        bus.noteTcpTxOk();
        n++;
        if (offlineDrain_) {
            delay(2);
            yield();
        } else if ((n & 3) == 0) {
            yield();
        }
    }
}
