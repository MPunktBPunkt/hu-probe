#pragma once

/**
 * @file PanicTail.h
 * @brief RTC-retained lifecycle breadcrumbs and restart cause for post-reset diagnosis.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Small RTC slow-memory tail for post-reset diagnosis.
 *
 * Writes never touch flash and are deliberately limited to coarse lifecycle /
 * USB epoch breadcrumbs. The retained copy is exposed after the next boot.
 * Holds up to 32 breadcrumbs (ring, oldest overwritten).
 */
class PanicTail {
public:
    /** @brief Breadcrumb codes (emitted as numeric "code" in panic_tail). */
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

    /** @brief Breadcrumb arg for UploadDone. */
    enum UploadResult : uint8_t { UploadDelivered = 1, UploadQueued = 2, UploadSessionFailed = 3 };

    /** @brief Breadcrumb arg for CoexHold: why the WiFi/USB coexistence hold was taken. */
    enum CoexReason : uint8_t {
        CoexOther = 0,
        CoexAssocLost = 1,
        CoexStallUsb = 2,
        CoexStallSoft = 3,
        CoexZombie = 4,
    };

    /** @brief Firmware-initiated restart cause, retained across the reset (see noteRestart()). */
    enum RestartCause : uint8_t {
        RestartNone = 0,
        RestartWifiUnrecoverable = 1,
        RestartPortalTimeout = 2,
        RestartHubOta = 3,
        RestartWebUi = 4,
        RestartGithubOta = 5,
        RestartOtaRollback = 6,
    };

    /**
     * @brief Capture the reset reason, keep a copy of the previous boot's tail and start a fresh one.
     * @note Call once early in boot, before any breadcrumb().
     */
    static void begin();

    /**
     * @brief Store the session id in the tail (max. 63 chars) and add a SessionStart breadcrumb.
     * @param sid  Session id; nullptr is ignored.
     */
    static void setSession(const char* sid);

    /**
     * @brief Append a breadcrumb with the current millis().
     *
     * Repeated USB chatter (UsbMount, UsbUmount, EpStall, BotReset, NeedReset) with the same
     * code and arg is folded into the last crumb (repeat counter, time updated).
     *
     * @param code  Lifecycle event code.
     * @param arg   Code-specific argument (see Code comments).
     */
    static void breadcrumb(Code code, uint32_t arg = 0);

    /**
     * @brief Breadcrumb + cause that survives the reset; persist a DiagLog snapshot, then return.
     *
     * Callers still have to ESP.restart(). The snapshot is forced (no rate limit) so a
     * planned reboot leaves a trail that power-on would otherwise wipe from RTC.
     *
     * @param cause  Reason for the upcoming restart.
     */
    static void noteRestart(RestartCause cause);

    /**
     * @brief Add boot info and, if present, the previous boot's tail as "panic_tail".
     *
     * panic_tail holds session_id, generation and the breadcrumbs (code, t_ms, arg, repeat)
     * oldest first.
     *
     * @param obj  Target JSON object.
     */
    static void appendPrevious(JsonObject obj);

    /**
     * @brief Add this boot's breadcrumbs as "panic_now" (same shape as panic_tail).
     * @param obj  Target JSON object.
     */
    static void appendCurrent(JsonObject obj);

    /**
     * @brief Format breadcrumbs of the previous or current boot as text lines.
     *
     * @param[out] dst  Destination buffer; always NUL-terminated when n > 0.
     * @param n         Capacity of dst in bytes.
     * @param previous  true = last boot (empty if RTC was lost, e.g. power-on).
     * @return Bytes written, not counting the NUL.
     */
    static size_t formatBreadcrumbs(char* dst, size_t n, bool previous);

    /**
     * @brief Add reset_reason, restart_cause, boot_generation, uptime_ms.
     * @param obj  Target JSON object.
     */
    static void appendBootInfo(JsonObject obj);

    /**
     * @brief Return the hardware reset reason of this boot.
     * @return e.g. "power_on", "software", "panic", "task_watchdog", "brownout" or "unknown".
     */
    static const char* resetReason();

    /**
     * @brief Cause recorded by the previous boot before its ESP.restart() ("none" otherwise).
     * @return e.g. "hub_ota", "github_ota", "ota_rollback", "webui" or "none".
     */
    static const char* restartCause();

    /**
     * @brief 1 after power-on, +1 on every reset that kept RTC memory (sw, panic, watchdog).
     * @return Boot generation counter.
     */
    static uint32_t bootGeneration();

private:
    PanicTail() = delete;
};
