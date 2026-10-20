#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class ConfigStore;
class Session;
class MscGadget;
class ReadInstrument;
class TcpStreamer;
class StatusLed;

/**
 * Drive mode (0.3.0, config autoCapture): every boot is a potential trip.
 *   Boot      MSC soft-detached; WLAN (home or phone hotspot, no portal) for ≤15 s;
 *             upload stored trips if the collector answers; GitHub update check;
 *             pre-erase flash; then arm the offline capture with flash journal.
 *   WaitHost  capture runs, soft re-enum done; no host mount within 20 s → charger,
 *             drop the run and continue as a normal (WLAN) probe.
 *   Capturing until power loss, max duration or stop; then upload or keep it in flash.
 *   CarIdle   run ended without WLAN: WiFi stays off (no restart loop in the car).
 *   Normal    regular probe behaviour (live session, App WiFi supervision).
 */
class DriveMode {
public:
    enum class State : uint8_t { Off, Boot, WaitHost, Capturing, CarIdle, Normal };

    static DriveMode& instance();

    void begin(ConfigStore* config, Session* session, MscGadget* msc, ReadInstrument* inst,
               TcpStreamer* tcp, StatusLed* led);
    bool enabled() const { return state_ != State::Off; }
    State state() const { return state_; }
    const char* stateName() const;

    bool hasCredentials() const;
    /** Home WLAN (WiFiManager store) or hotspot, strongest wins. Never opens the portal. */
    bool connectWifi(uint32_t timeoutMs);
    /** Blocking part after the WLAN attempt: uploads, update, arm. */
    void bootPhase();
    void poll();
    /** App must not supervise/restart WiFi in these states. */
    bool ownsWifi() const;
    void toJson(JsonObject obj) const;

private:
    DriveMode() = default;
    static bool connectThunk(uint32_t timeoutMs);
    bool collectorReachable() const;
    void uploadPending();
    void armCapture();
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
    uint8_t uploaded_ = 0;
    unsigned long hostDeadlineMs_ = 0;
};
