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
    };

    static void begin();
    static void setSession(const char* sid);
    static void breadcrumb(Code code, uint32_t arg = 0);
    static void appendPrevious(JsonObject obj);
    static const char* resetReason();

private:
    PanicTail() = delete;
};
