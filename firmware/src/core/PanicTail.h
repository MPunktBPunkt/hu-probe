#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Small RTC slow-memory tail for post-reset diagnosis.
 *
 * Writes never touch flash and are deliberately limited to coarse lifecycle /
 * USB epoch breadcrumbs. The retained copy is exposed after the next boot.
 */
class PanicTail {
public:
    enum Code : uint8_t {
        SessionStart = 1,
        OfflineArm = 2,
        OfflineCapture = 3,
        MediaHide = 4,
        SoftDetach = 5,
        WifiUpload = 6,
        UsbMount = 7,
        UsbUmount = 8,
        BotReset = 9,
        NeedReset = 10,
        EpStall = 11,
        MediaSwitch = 12,  // arg: 1=enter 2=select_ok 3=msc_begin 4=done; 0x10|step on fail
        UploadDone = 13,   // arg: UploadResult
        MediaRestore = 14,
        WifiLost = 15,
        CoexHold = 16,     // arg: CoexReason
        CoexReattach = 17,
        ZombieRestart = 18,
        RestartRequested = 19,  // arg: RestartCause
    };

    enum UploadResult : uint8_t { UploadDelivered = 1, UploadQueued = 2, UploadSessionFailed = 3 };

    enum CoexReason : uint8_t {
        CoexOther = 0,
        CoexAssocLost = 1,
        CoexStallUsb = 2,
        CoexStallSoft = 3,
        CoexZombie = 4,
    };

    enum RestartCause : uint8_t {
        RestartNone = 0,
        RestartWifiUnrecoverable = 1,
        RestartPortalTimeout = 2,
        RestartHubOta = 3,
        RestartWebUi = 4,
        RestartGithubOta = 5,
        RestartOtaRollback = 6,
    };

    static void begin();
    static void setSession(const char* sid);
    static void breadcrumb(Code code, uint32_t arg = 0);
    /** Breadcrumb + cause that survives the reset; call right before ESP.restart(). */
    static void noteRestart(RestartCause cause);
    static void appendPrevious(JsonObject obj);
    /** reset_reason, restart_cause, boot_generation, uptime_ms. */
    static void appendBootInfo(JsonObject obj);
    static const char* resetReason();
    /** Cause recorded by the previous boot before its ESP.restart() ("none" otherwise). */
    static const char* restartCause();
    /** 1 after power-on, +1 on every reset that kept RTC memory (sw, panic, watchdog). */
    static uint32_t bootGeneration();

private:
    PanicTail() = delete;
};
