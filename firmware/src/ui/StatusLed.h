#pragma once

/**
 * @file StatusLed.h
 * @brief On-board RGB status LED showing the probe state at a glance.
 */

#include <Arduino.h>

class MscGadget;
class Session;
class ReadInstrument;
class TcpStreamer;
class EventBus;

/**
 * @brief Drives the on-board RGB LED from session, MSC, TCP and EventBus state.
 *
 * On-board RGB (ESP32-S3 DevKit RGB_BUILTIN) — field glanceable probe state.
 *
 * Priority (highest wins):
 *   red blink     — capture unhealthy (drops / MSC sample overflow)
 *   magenta blink — TinyUSB MSC_STAGE_NEED_RESET
 *   orange blink  — USB plugged but TCP to collector down
 *   green solid   — host actively reading (measurement alive)
 *   cyan solid    — USB plugged, quiet / enum
 *   blue dim      — WiFi OK, stick not plugged (waiting for HU)
 *   blue blink    — stored-trip upload (USB soft-detached on purpose; not NeedReset)
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
    /** @brief Blocking drive-mode boot step shown directly by showDrivePhase(). */
    enum class DrivePhase : uint8_t { None, Check, Upload, Ota };

    /** @brief Show the white boot flash (~80 ms, blocking) and enter boot mode. */
    void begin();

    /**
     * @brief Flag the WiFi portal / connecting state.
     *
     * When switched on, writes yellow immediately because setupWifi() blocks in autoConnect
     * and tick() does not run meanwhile.
     *
     * @param on true while the WiFiManager portal or connect is running.
     */
    void setWifiPortal(bool on);

    /**
     * @brief Flag the coex hold (USB soft-detached so WiFi can recover); shown as yellow/cyan.
     * @param on true while the hold is active. Stored in a volatile flag.
     */
    void setCoexHold(bool on) { coexHold_ = on; }

    /**
     * @brief Show a drive-mode boot phase colour immediately (dim white, blue, bright white; off for None).
     * @param p Phase to display.
     */
    void showDrivePhase(DrivePhase p);

    /**
     * @brief Flag a running drive-mode capture (purple with a white flash every 2 s).
     * @param on true while the drive capture is running.
     */
    void setDriveCapture(bool on) { driveCapture_ = on; }

    /**
     * @brief Flag a full trip journal (red/purple blink).
     * @param on true if no new drive capture can start.
     */
    void setTripsFull(bool on) { tripsFull_ = on; }

    /**
     * @brief Flag a stored-trip upload job (suppresses NeedReset magenta after soft-detach).
     * @param on true while WebUi trip-upload or stored replay is running.
     */
    void setTripUpload(bool on) { tripUpload_ = on; }

    /**
     * @brief Select the highest-priority LED mode from the current state and update the LED.
     *
     * A host read counts as "recent" for 1.5 s. TCP is considered down when data is pending but
     * no tcp_tx_ok was seen for 8 s (except the first 30 s after boot) or when USB is plugged and
     * the collector socket is not connected.
     *
     * @param session    Live session (currently unused).
     * @param msc        MSC gadget (plug state, sample overflow).
     * @param instrument Read instrumentation (read count for activity detection).
     * @param tcp        TCP streamer (collector socket state).
     * @param bus        Event bus (drops, pending events, last tcp_tx_ok).
     * @note Call periodically from the main loop; blink timing depends on the call rate.
     */
    void tick(const Session& session, const MscGadget& msc, const ReadInstrument& instrument,
              const TcpStreamer& tcp, const EventBus& bus);

private:
    /** @brief Displayed LED state, ordered by tick() priority (see class description). */
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
        TripsFull,
        TripUpload  ///< dim blue blink — WiFi TX of stored trips (USB intentionally detached)
    };

    /**
     * @brief Write a raw colour to RGB_BUILTIN via neopixelWrite (no-op on boards without RGB_BUILTIN).
     * @param r Red intensity, 0..255.
     * @param g Green intensity, 0..255.
     * @param b Blue intensity, 0..255.
     */
    void write(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Switch to a mode and render it, toggling the blink phase.
     *
     * Blink period is 200 ms for Unhealthy, NeedReset and SeqPrompt, 400 ms otherwise; a mode
     * change restarts the blink with the LED on.
     *
     * @param mode Mode to display.
     * @param now  Current millis() timestamp.
     */
    void apply(Mode mode, unsigned long now);

    Mode mode_ = Mode::Off;
    bool wifiPortal_ = false;
    volatile bool coexHold_ = false;
    bool driveCapture_ = false;
    bool tripsFull_ = false;
    bool tripUpload_ = false;
    uint32_t lastReadCount_ = 0;
    unsigned long lastReadMs_ = 0;
    unsigned long lastBlinkMs_ = 0;
    bool blinkOn_ = false;
};
