#pragma once

/**
 * @file App.h
 * @brief Top-level application singleton: boot sequence, main loop and WiFi/USB coex supervision.
 */

#include "core/ConfigStore.h"
#include "core/HubClient.h"
#include "session/Session.h"
#include "msc/MscGadget.h"
#include "msc/ReadInstrument.h"
#include "net/TcpStreamer.h"
#include "net/EventBus.h"
#include "web/WebUi.h"
#include "ui/StatusLed.h"

/**
 * @brief Application singleton that owns all subsystems (config, hub, session, MSC, TCP, web UI, LED).
 *
 * Wires the subsystems together at boot, drives them from the Arduino loop and supervises
 * WiFi from a dedicated FreeRTOS task (coex soft-detach of USB, reconnect, restart watchdog).
 */
class App {
public:
    /**
     * @brief Return the process-wide App instance (function-local static, created on first call).
     * @return Reference to the singleton.
     */
    static App& instance();

    /**
     * @brief Boot all subsystems: serial, diagnostics, OTA boot check, config, event bus, MSC, WiFi, hub, TCP, web UI.
     *
     * MSC is started before WiFi so the HU sees a stick even while the WiFi portal runs; in
     * drive mode (config autoCapture) MSC stays detached until DriveMode arms the capture and
     * DriveMode::bootPhase() runs instead of the live auto-session. Holding the reset button at
     * boot in drive mode forces the WiFiManager portal. lab_debian keeps MSC soft-detached by
     * default. Finally spawns wifiCoexTask (prio 25, core 0).
     *
     * @note Called once from Arduino setup(); may block (WiFi connect/portal) and may restart
     *       the chip on portal timeout.
     */
    void begin();

    /**
     * @brief Run one iteration of the main loop: poll MSC, capture, drive mode, sequencer, TCP, web, hub and LED.
     *
     * Publishes status every STATUS_INTERVAL_MS and capture_health every
     * CAPTURE_HEALTH_INTERVAL_MS (both doubled while a USB host is plugged) unless a coex hold
     * is active. The hub is polled at most every 60 s while a USB host is plugged, because its
     * HTTPClient can block up to 2 s. WiFi supervision does not run here but in wifiCoexTask.
     *
     * @note Called from Arduino loop() only.
     */
    void loop();

    /** @brief Mutable access to the persistent configuration store. */
    ConfigStore& config() { return config_; }

    /** @brief Mutable access to the live capture session. */
    Session& session() { return session_; }

    /** @brief Mutable access to the read-rate / TUR instrumentation. */
    ReadInstrument& instrument() { return instrument_; }

private:
    App() = default;

    /**
     * @brief Connect WiFi via WiFiManager and apply the coex-safe radio settings.
     *
     * After a successful connect: modem sleep off (WIFI_PS_NONE), TX power 19.5 dBm,
     * auto-reconnect off (superviseWifi owns reconnects). On autoConnect/portal timeout without
     * forcePortal the chip restarts; with forcePortal an abandoned portal just returns so the
     * drive trip is not lost.
     *
     * @param forcePortal Open the WiFiManager portal even with stored credentials (BOOT at boot).
     */
    void setupWifi(bool forcePortal = false);

    /** @brief Start mDNS as "huprobe-XXXXXX.local" (last 3 MAC bytes) with an HTTP service on port 80, if enabled in config. */
    void setupMdns();

    /**
     * @brief Fill the hub heartbeat payload with identity, firmware/hardware info and live IOS stats.
     * @param[out] doc JSON document to populate (mac, ip, name, fw*, chip/hw, and an "ios" object).
     */
    static void buildHeartbeat(JsonDocument& doc);

    /**
     * @brief Publish a v2 "status" event (read rate, TUR rate, phase, scenario, vehicle) to the EventBus.
     *
     * Does nothing without an active session or while an offline capture owns the session/TCP path.
     */
    void emitStatus();

    /**
     * @brief Publish a v2 "capture_health" event (ring/seq stats, overflows, transport, panic/diag tails, build info).
     *
     * Also stores last_seq/last_sid to NVS at most every NVS_TAIL_INTERVAL_MS (~60 s), because
     * flash erase during MSC+WiFi was a field WiFi-killer. Does nothing without an active session
     * or while an offline capture is active.
     */
    void emitCaptureHealth();

    /**
     * @brief Supervise the WiFi link: detect TCP stall/zombie association, run coex hold, reconnect and restart watchdog.
     *
     * A TCP stall (queue at/above threshold, no tcp_tx_ok for the stall time after TX worked at
     * least once and the post-connect grace) enters a coex hold. While in hold, USB is
     * reattached once the ring is quiet and data has flowed for WIFI_COEX_FLUSH_MS (never for the
     * lab_debian default detach); a zombie association restarts the STA. Lost association
     * triggers reconnect attempts every WIFI_RECONNECT_EVERY_MS and, after the restart timeout
     * and at least 3 attempts, ESP.restart().
     *
     * @note Runs in wifiCoexTask, not in the Arduino loop.
     */
    void superviseWifi();

    /**
     * @brief During trip/OC upload: reconnect only if association is lost.
     *
     * Must not call enterCoexHold / esp_wifi_stop or touch TcpStreamer — those race the
     * main-loop owner during stored replay.
     */
    void softWifiKeepalive();

    /**
     * @brief Soft-detach USB + hard WiFi stop/start (lab OTG coex).
     *
     * Records a PanicTail breadcrumb, sets the coex-hold flag and resets the recovery/zombie
     * clocks. The EventBus ring is only cleared when no session is active (B23: never wipe live
     * session evidence). Performs a full esp_wifi_stop()/start() because a soft reconnect is not
     * enough after OTG RF death; blocks ~220 ms.
     *
     * @param reason Short cause tag mapped to a PanicTail code ("assoc-lost", "stall+usb",
     *               "stall+soft", "zombie-assoc"; anything else or nullptr = other).
     */
    void enterCoexHold(const char* reason);

    /**
     * @brief Check whether collector TX progressed recently (real IP path).
     * @param now Current millis() timestamp.
     * @return true if the collector is disabled or the last tcp_tx_ok is younger than
     *         WIFI_TCP_ALIVE_MS; false if TX never succeeded or is older.
     */
    bool tcpDataAlive(unsigned long now) const;

    /**
     * @brief Check whether the link counts as alive: tcpDataAlive, or nothing queued.
     *
     * Without a session nothing is sent, so a stale tcp_tx_ok alone must not count as a zombie
     * link (post-upload restart loop). A socket is not required: a stopped collector is no
     * reason to restart the chip.
     *
     * @param now Current millis() timestamp.
     * @return true if data flowed recently or the EventBus has no pending events.
     */
    bool linkAlive(unsigned long now) const;

    /**
     * @brief Check whether the configured vehicle or profile is "lab_debian".
     * @return true for the lab_debian setup (MSC soft-detached by default, no auto-reattach).
     */
    bool isLabDebian() const;

    /**
     * @brief FreeRTOS task loop: call superviseWifi() and flush TCP during coex hold every 500 ms.
     *
     * Runs off Arduino loop — USB load was starving coex soft-detach (0.2.16). Created at
     * prio 25 (above TinyUSB usbd, prio 24) on core 0. Skips supervision while OfflineCapture
     * or DriveMode owns WiFi.
     *
     * @param arg Pointer to the owning App instance.
     */
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
