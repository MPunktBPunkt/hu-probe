/**
 * @file StatusLed.cpp
 * @brief LED mode selection (priority order) and colour/blink rendering.
 */

#include "StatusLed.h"
#include "session/Session.h"
#include "msc/MscGadget.h"
#include "msc/ReadInstrument.h"
#include "msc/MscBotTap.h"
#include "capture/OfflineCapture.h"
#include "capture/SequenceRunner.h"
#include "net/TcpStreamer.h"
#include "net/EventBus.h"
#include <WiFi.h>

#if defined(RGB_BUILTIN)
#include "esp32-hal-rgb-led.h"
#endif

void StatusLed::begin() {
    write(40, 40, 40);  // white boot flash
    delay(80);
    write(0, 0, 0);
    mode_ = Mode::Boot;
}

void StatusLed::setWifiPortal(bool on) {
    wifiPortal_ = on;
    if (on) {
        // setupWifi() blocks in autoConnect — show yellow immediately.
        write(50, 35, 0);
        mode_ = Mode::WifiPortal;
    }
}

void StatusLed::showDrivePhase(DrivePhase p) {
    switch (p) {
        case DrivePhase::Check: write(18, 18, 18); break;
        case DrivePhase::Upload: write(0, 0, 50); break;
        case DrivePhase::Ota: write(50, 50, 50); break;
        default: write(0, 0, 0); break;
    }
    mode_ = Mode::Boot;
}

void StatusLed::write(uint8_t r, uint8_t g, uint8_t b) {
#if defined(RGB_BUILTIN)
    neopixelWrite(RGB_BUILTIN, r, g, b);
#else
    (void)r;
    (void)g;
    (void)b;
#endif
}

void StatusLed::tick(const Session& session, const MscGadget& msc, const ReadInstrument& instrument,
                     const TcpStreamer& tcp, const EventBus& bus) {
    unsigned long now = millis();

    uint32_t reads = instrument.readCount();
    if (reads != lastReadCount_) {
        lastReadCount_ = reads;
        lastReadMs_ = now;
    }
    bool recentRead = (lastReadMs_ > 0) && (now - lastReadMs_ < 1500);

    Mode next = Mode::IdleWifi;
    // Stored trip upload: soft-detach latches NEED_RESET — show upload blue, not magenta.
    if (tripUpload_ || OfflineCapture::instance().replayingStored()) {
        next = Mode::TripUpload;
    } else if (OfflineCapture::instance().active()) {
        // Offline: green while host reads, purple otherwise (WiFi intentionally off).
        next = (msc.plugged() && recentRead) ? Mode::Measuring
               : driveCapture_                ? Mode::DriveCapture
                                              : Mode::OfflineCapture;
    } else if (SequenceRunner::instance().waitingForDriver()) {
        next = Mode::SeqPrompt;
    } else if (tripsFull_) {
        next = Mode::TripsFull;
    } else if (coexHold_ && !wifiPortal_) {
        next = Mode::CoexHold;
    } else if (wifiPortal_ || WiFi.status() != WL_CONNECTED) {
        next = Mode::WifiPortal;
    } else if (bus.dropped() > 0 || msc.sampleOverflow() > 0) {
        next = Mode::Unhealthy;
    } else if (MscBotTap::needResetLatched()) {
        next = Mode::NeedReset;
    } else {
        // Assoc alone is not IdleWifi — zombie WL_CONNECTED (no recent tcp_tx_ok while data
        // waits). Nothing queued (no session after upload) is idle, same as App::linkAlive.
        const unsigned long lastOk = bus.lastTcpTxOkMs();
        const bool dataAlive = (lastOk != 0) && (now - lastOk < 8000UL);
        const bool bootGrace = (lastOk == 0) && (now < 30000UL);
        if (!dataAlive && !bootGrace && bus.pending() > 0) {
            next = Mode::TcpDown;
        } else if (msc.plugged() && !tcp.connected()) {
            next = Mode::TcpDown;
        } else if (msc.plugged() && recentRead) {
            next = Mode::Measuring;
        } else if (msc.plugged()) {
            next = Mode::UsbQuiet;
        } else {
            next = Mode::IdleWifi;
        }
    }
    (void)session;
    apply(next, now);
}

void StatusLed::apply(Mode mode, unsigned long now) {
    const unsigned blinkMs =
        (mode == Mode::Unhealthy || mode == Mode::NeedReset || mode == Mode::SeqPrompt) ? 200
                                                                                         : 400;

    if (mode != mode_) {
        mode_ = mode;
        blinkOn_ = true;
        lastBlinkMs_ = now;
    } else if (now - lastBlinkMs_ >= blinkMs) {
        lastBlinkMs_ = now;
        blinkOn_ = !blinkOn_;
    }

    switch (mode_) {
        case Mode::Boot:
            write(40, 40, 40);
            break;
        case Mode::WifiPortal:
            write(blinkOn_ ? 50 : 0, blinkOn_ ? 35 : 0, 0);  // yellow
            break;
        case Mode::IdleWifi:
            write(0, 0, 18);  // dim blue — waiting for plug
            break;
        case Mode::UsbQuiet:
            write(0, 28, 32);  // cyan — host present
            break;
        case Mode::Measuring:
            write(0, 45, 8);  // green — HU reading
            break;
        case Mode::TcpDown:
            write(blinkOn_ ? 50 : 0, blinkOn_ ? 18 : 0, 0);  // orange
            break;
        case Mode::NeedReset:
            write(blinkOn_ ? 45 : 0, 0, blinkOn_ ? 40 : 0);  // magenta
            break;
        case Mode::Unhealthy:
            write(blinkOn_ ? 55 : 0, 0, 0);  // red
            break;
        case Mode::OfflineCapture:
            write(28, 0, 40);  // purple — WiFi OFF, RAM capture
            break;
        case Mode::DriveCapture:
            if (now % 2000 < 120) write(40, 40, 40);
            else write(28, 0, 40);
            break;
        case Mode::TripsFull:
            if (blinkOn_) write(55, 0, 0);
            else write(28, 0, 40);
            break;
        case Mode::TripUpload:
            // Dim blue blink — intentional USB detach, WiFi TX in progress.
            write(0, 0, blinkOn_ ? 28 : 10);
            break;
        case Mode::CoexHold:
            if (blinkOn_) write(50, 35, 0);  // yellow
            else write(0, 28, 32);           // cyan
            break;
        case Mode::SeqPrompt:
            write(blinkOn_ ? 45 : 0, blinkOn_ ? 45 : 0, blinkOn_ ? 45 : 0);  // white — driver
            break;
        case Mode::Off:
        default:
            write(0, 0, 0);
            break;
    }
}
