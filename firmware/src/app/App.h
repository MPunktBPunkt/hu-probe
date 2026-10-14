#pragma once

#include "core/ConfigStore.h"
#include "core/HubClient.h"
#include "session/Session.h"
#include "msc/MscGadget.h"
#include "msc/ReadInstrument.h"
#include "net/TcpStreamer.h"
#include "net/EventBus.h"
#include "web/WebUi.h"
#include "ui/StatusLed.h"

class App {
public:
    static App& instance();
    void begin();
    void loop();

    ConfigStore& config() { return config_; }
    Session& session() { return session_; }
    ReadInstrument& instrument() { return instrument_; }

private:
    App() = default;
    void setupWifi();
    void setupMdns();
    static void buildHeartbeat(JsonDocument& doc);
    void emitStatus();
    void emitCaptureHealth();
    void superviseWifi();

    ConfigStore config_;
    HubClient hub_;
    Session session_;
    MscGadget msc_;
    ReadInstrument instrument_;
    TcpStreamer tcp_;
    WebUi web_;
    StatusLed led_;
    unsigned long lastStatusMs_ = 0;
    unsigned long lastCaptureHealthMs_ = 0;
    unsigned long lastNvsTailMs_ = 0;
    unsigned long wifiDownSinceMs_ = 0;
    unsigned long lastWifiReconnectMs_ = 0;
    uint8_t wifiReconnectAttempts_ = 0;
};
