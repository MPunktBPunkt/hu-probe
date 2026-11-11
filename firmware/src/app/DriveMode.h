#pragma once

/**
 * @file DriveMode.h
 * @brief Drive mode state machine: unattended in-car trip capture with flash journal and upload at boot.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

class ConfigStore;
class Session;
class MscGadget;
class ReadInstrument;
class TcpStreamer;
class StatusLed;

/**
 * @brief Singleton state machine that turns every boot into a potential recorded trip.
 *
 * Drive mode (0.3.0, config autoCapture): every boot is a potential trip until home clear.
 *   Boot      MSC soft-detached; WLAN (home or phone hotspot, no portal) for ≤15 s;
 *             upload stored trips if the collector answers; field plan fetch; GitHub
 *             update check.
 *             Home WLAN: no new capture unless the field plan sets arm_at_home; without an
 *             active plan, collector ok and journal empty → clear autoCapture (one-shot) and
 *             go Normal. Else (hotspot / no WLAN): pre-erase and arm the offline capture with
 *             flash journal, using the plan's next job if a plan is active.
 *   WaitHost  capture runs, soft re-enum done; no host mount within 20 s → charger,
 *             drop the run and continue as a normal (WLAN) probe.
 *   Capturing until power loss, max duration or stop; then upload or keep it in flash.
 *   CarIdle   run ended (or online_only plan blocked it) without WLAN: WiFi stays off (no
 *             restart loop in the car).
 *   Normal    regular probe behaviour (live session, App WiFi supervision).
 */
class DriveMode {
public:
    /** @brief Drive mode phase; see the class description for the meaning of each state. */
    enum class State : uint8_t { Off, Boot, WaitHost, Capturing, CarIdle, Normal };

    /**
     * @brief Return the process-wide DriveMode instance (function-local static).
     * @return Reference to the singleton.
     */
    static DriveMode& instance();

    /**
     * @brief Enter State::Boot: store the subsystem pointers and read the home WLAN credentials.
     *
     * WiFiManager keeps the home WLAN in the esp_wifi NVS config; it is read here before any
     * WiFi.begin(ssid, pass) replaces the RAM copy. Also initialises the TripStore and shows the
     * "check" LED phase.
     *
     * @param config  Configuration store (hotspot credentials, collector, experiment, media); must outlive DriveMode.
     * @param session Live session started when falling back to normal mode.
     * @param msc     MSC gadget (soft attach/detach, media switch, plug state).
     * @param inst    Read instrumentation passed to the offline capture.
     * @param tcp     TCP streamer used for trip upload.
     * @param led     Status LED for drive phase / trip indications.
     */
    void begin(ConfigStore* config, Session* session, MscGadget* msc, ReadInstrument* inst,
               TcpStreamer* tcp, StatusLed* led);

    /** @brief True once begin() was called (any state other than State::Off). */
    bool enabled() const { return state_ != State::Off; }

    /** @brief Current drive mode phase. */
    State state() const { return state_; }

    /**
     * @brief Return the current state as a snake_case string for JSON/logs.
     * @return "off", "boot", "wait_host", "capturing", "car_idle", "normal" (or "?" for unknown values).
     */
    const char* stateName() const;

    /**
     * @brief Check whether any WLAN credentials are known.
     * @return true if a home SSID (from the WiFiManager store) or a hotspot SSID (config wifi2Ssid) is set.
     */
    bool hasCredentials() const;

    /**
     * @brief Connect to the home WLAN (WiFiManager store) or hotspot, strongest wins. Never opens the portal.
     *
     * Uses WiFiMulti with persistent(false) so the hotspot never overwrites the home WLAN in NVS.
     * On success applies modem sleep off, TX power 19.5 dBm and auto-reconnect off. Blocks up to
     * timeoutMs.
     *
     * @param timeoutMs Maximum connect time in milliseconds.
     * @return true if already connected or connected within the timeout; false without
     *         credentials or on timeout.
     */
    bool connectWifi(uint32_t timeoutMs);

    /**
     * @brief Blocking part after the WLAN attempt: uploads, update, arm — or one-shot clear.
     *
     * Marks the OTA image healthy first (pulling the plug during an upload must not trigger a
     * rollback), waits up to 3 s for NTP, uploads pending trips if the collector is reachable,
     * refreshes the field plan, runs the GitHub update check if autoUpdate is set (restarts on
     * update).
     *
     * On **home WLAN**: arms a new capture only if the active field plan sets arm_at_home
     * (car parked within reach of the home WLAN); otherwise normal probe mode. Without an
     * active plan, a reached collector and an empty journal clear config autoCapture (one-shot
     * Heimfahrt). Hotspot or no WLAN arms the offline capture, unless the plan sets online_only
     * and was not fetched on this boot: then no capture (CarIdle without WLAN, else normal mode).
     */
    void bootPhase();

    /**
     * @brief Advance the state machine; call from the main loop.
     *
     * WaitHost: switches to Capturing once the host mounts (or the capture stopped by itself);
     * after kHostWaitMs without a mount (charger) aborts the run and falls back to normal mode.
     * Capturing: when the capture ends, goes to normal mode with WLAN, otherwise stops WiFi and
     * enters CarIdle.
     */
    void poll();

    /**
     * @brief Tell App whether drive mode currently owns the WiFi radio.
     *
     * App must not supervise/restart WiFi in these states.
     *
     * @return true in Boot, WaitHost, Capturing and CarIdle.
     */
    bool ownsWifi() const;

    /**
     * @brief Write the drive mode status (state, WLAN, collector, trips, OTA info, last error) into a JSON object.
     * @param[out] obj Target JSON object.
     */
    void toJson(JsonObject obj) const;

private:
    DriveMode() = default;

    /**
     * @brief Static adapter for OfflineCapture::setWifiConnector(); forwards to instance().connectWifi().
     * @param timeoutMs Maximum connect time in milliseconds.
     * @return Result of connectWifi().
     */
    static bool connectThunk(uint32_t timeoutMs);

    /**
     * @brief Probe the collector with a TCP connect (1.5 s timeout).
     * @return true if the collector is enabled, WiFi is connected and the connect succeeded.
     */
    bool collectorReachable() const;

    /**
     * @brief True when associated to the WiFiManager home SSID (not the phone hotspot).
     */
    bool onHomeWifi() const;

    /**
     * @brief Persist autoCapture=false and log why (home one-shot clear).
     * @param why Short reason for DiagLog.
     */
    void disableAutoCapture(const char* why);

    /**
     * @brief Replay up to 8 stored trips to the collector, each bounded by kTripUploadMaxMs.
     *
     * Stops and sets lastError_ = "trip_upload_incomplete" if a trip is still pending after its
     * replay; increments uploaded_ per completed trip.
     */
    void uploadPending();

    /**
     * @brief Arm the persistent offline capture for the configured experiment and enter WaitHost.
     *
     * An active field plan supplies experiment, media, fine mode, measure time and the
     * scenario "fp_<plan>_<job>_r<rep>" instead of the config, and optionally a READ10 brake
     * (ReadStall, ends with the capture; a rejected brake config leaves the run uncounted).
     * Falls back to "drive_home",
     * then "free" if the experiment is unknown; switches media if needed. On a full trip journal, missing experiment or arm failure, MSC is
     * reattached and the state becomes Normal/CarIdle (lastError_ set accordingly). The capture
     * uses soft USB re-enum, so it is the HU's first sight of the stick.
     */
    void armCapture();

    /**
     * @brief Fall back to normal probe behaviour: connect WiFi (up to kWifiBootMs), attach MSC, start a live session.
     */
    void toNormal();

    static constexpr uint32_t kWifiBootMs = 15000;
    static constexpr uint32_t kHostWaitMs = 20000;
    static constexpr size_t kPreEraseBytes = 256 * 1024;
    static constexpr uint32_t kPreEraseMaxMs = 8000;
    static constexpr uint32_t kTripUploadMaxMs = 600000;

    ConfigStore* config_ = nullptr;
    Session* session_ = nullptr;
    MscGadget* msc_ = nullptr;
    ReadInstrument* inst_ = nullptr;
    TcpStreamer* tcp_ = nullptr;
    StatusLed* led_ = nullptr;
    State state_ = State::Off;
    String homeSsid_;
    String homePass_;
    String lastSsid_;
    String otaInfo_;
    String lastError_;
    bool collectorSeen_ = false;
    bool planRun_ = false;  // armed capture belongs to the field plan; counted on host mount
    uint8_t uploaded_ = 0;
    unsigned long hostDeadlineMs_ = 0;
};
