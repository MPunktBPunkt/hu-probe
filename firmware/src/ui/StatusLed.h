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
 *   yellow blink  — WiFi portal / connecting
 *   white flash   — boot
 */
class StatusLed {
public:
    void begin();
    void setWifiPortal(bool on);
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
        Unhealthy
    };

    void write(uint8_t r, uint8_t g, uint8_t b);
    void apply(Mode mode, unsigned long now);

    Mode mode_ = Mode::Off;
    bool wifiPortal_ = false;
    uint32_t lastReadCount_ = 0;
    unsigned long lastReadMs_ = 0;
    unsigned long lastBlinkMs_ = 0;
    bool blinkOn_ = false;
};
