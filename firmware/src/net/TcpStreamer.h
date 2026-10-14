#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include "core/ConfigStore.h"

/** TCP-Client → Collector :9100, JSONL Zeilen. */
class TcpStreamer {
public:
    void begin(ConfigStore* config);
    void loop();
    bool connected() const { return client_.connected(); }

private:
    void tryConnect();
    void flushPending();

    ConfigStore* config_ = nullptr;
    WiFiClient client_;
    unsigned long lastConnectAttempt_ = 0;
};
