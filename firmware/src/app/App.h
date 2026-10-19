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
    /** Soft-detach USB + hard WiFi stop/start (lab OTG coex). */
    void enterCoexHold(const char* reason);
    /** True if collector TX progressed recently (real IP path). */
    bool tcpDataAlive(unsigned long now) const;
    bool isLabDebian() const;
    /** Runs off Arduino loop — USB load was starving coex soft-detach (0.2.16). */
    static void wifiCoexTask(void* arg);

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
    unsigned long lastHubWhileUsbMs_ = 0;
    unsigned long wifiDownSinceMs_ = 0;
    unsigned long lastWifiReconnectMs_ = 0;
    unsigned long wifiConnectedAtMs_ = 0;
    unsigned long coexRecoveredAtMs_ = 0;
    /** Separate zombie clock — must NOT depend on tcpOk (0.2.36 dead branch). */
    unsigned long coexZombieSinceMs_ = 0;
    uint8_t wifiReconnectAttempts_ = 0;
    /** Soft-detached USB so WiFi can recover (lab unattended OTG). */
    bool coexDetachedForWifi_ = false;
    /** lab_debian: MSC stays soft-detached unless explicitly armed. */
    bool labMscDetachedDefault_ = false;
    TaskHandle_t wifiCoexTaskHandle_ = nullptr;
};
