#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include "core/ConfigStore.h"

/** TCP-Client → Collector :9100, JSONL Zeilen. */
class TcpStreamer {
public:
    void begin(ConfigStore* config);
    void loop();
    bool connected() const { return const_cast<WiFiClient&>(client_).connected(); }
    /** Offline replay: smaller batches + TCP flush so WiFi TX completes. */
    void setOfflineDrain(bool on) { offlineDrain_ = on; }

private:
    void tryConnect();
    void flushPending();

    ConfigStore* config_ = nullptr;
    WiFiClient client_;
    unsigned long lastConnectAttempt_ = 0;
    bool wasConnected_ = false;
    bool offlineDrain_ = false;
};
