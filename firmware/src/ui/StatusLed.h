#pragma once

#include <Arduino.h>

class MscGadget;
class Session;
class ReadInstrument;
class TcpStreamer;
class EventBus;

/**
 * On-board RGB (ESP32-S3 DevKit RGB_BUILTIN) — field glanceable probe state.
 *
 * Priority (highest wins):
 *   red blink     — capture unhealthy (drops / MSC sample overflow)
 *   magenta blink — TinyUSB MSC_STAGE_NEED_RESET
 *   orange blink  — USB plugged but TCP to collector down
 *   green solid   — host actively reading (measurement alive)
 *   cyan solid    — USB plugged, quiet / enum
 *   blue dim      — WiFi OK, stick not plugged (waiting for HU)
 *   yellow/cyan   — coex hold: USB soft-detached so WiFi can recover
 *   yellow blink  — WiFi portal / connecting
 *   white flash   — boot
 *   purple solid  — Offline-Capture active (WiFi intentionally OFF)
 *   purple + white flash every 2 s — drive-mode capture (flash journal on)
 *   red/purple    — drive mode: trip journal full, no new capture
 *   white blink   — sequence waits for the driver (BOOT short = start step, long = skip)
 * Drive-mode boot phase (blocking, shown directly): dim white = WLAN/flash check,
 * blue = uploading stored trips, bright white = GitHub update.
 */
class StatusLed {
public:
    enum class DrivePhase : uint8_t { None, Check, Upload, Ota };

    void begin();
    void setWifiPortal(bool on);
    void setCoexHold(bool on) { coexHold_ = on; }
    void showDrivePhase(DrivePhase p);
    void setDriveCapture(bool on) { driveCapture_ = on; }
    void setTripsFull(bool on) { tripsFull_ = on; }
    void tick(const Session& session, const MscGadget& msc, const ReadInstrument& instrument,
              const TcpStreamer& tcp, const EventBus& bus);

private:
    enum class Mode : uint8_t {
        Off,
        Boot,
        WifiPortal,
        IdleWifi,
        UsbQuiet,
        Measuring,
        TcpDown,
        NeedReset,
        Unhealthy,
        OfflineCapture,
        SeqPrompt,
        CoexHold,
        DriveCapture,
        TripsFull
    };

    void write(uint8_t r, uint8_t g, uint8_t b);
    void apply(Mode mode, unsigned long now);

    Mode mode_ = Mode::Off;
    bool wifiPortal_ = false;
    volatile bool coexHold_ = false;
    bool driveCapture_ = false;
    bool tripsFull_ = false;
    uint32_t lastReadCount_ = 0;
    unsigned long lastReadMs_ = 0;
    unsigned long lastBlinkMs_ = 0;
    bool blinkOn_ = false;
};
