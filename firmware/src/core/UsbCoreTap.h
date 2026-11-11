#pragma once

/**
 * @file UsbCoreTap.h
 * @brief DCD-level USB tap: every event the controller driver hands to TinyUSB usbd.
 *
 * Linker wrap of `dcd_event_handler` (scripts/gate01b_link.py). Sees bus reset, suspend,
 * resume, unplug, SOF, every SETUP packet (standard, class, vendor; all recipients) and every
 * endpoint transfer completion — including requests TinyUSB answers internally
 * (GET_DESCRIPTOR, SET_CONFIGURATION, …) and requests it rejects with an EP0 STALL.
 *
 * Epistemic: what the DWC2 controller accepted, not wire packets. NAK, retries, CRC/PID
 * errors and tokens the controller rejects in hardware stay invisible.
 */

#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>

/**
 * @brief Static counters (always on) plus an optional per-event ring for publishing.
 */
class UsbCoreTap {
public:
    /** @brief Event type of a ring sample. */
    enum class Ev : uint8_t {
        BusReset = 1,
        Unplugged = 2,
        Suspend = 3,
        Resume = 4,
        Setup = 5,
        XferComplete = 6
    };

    /** @brief One DCD event captured in the USB interrupt and popped on the Arduino loop. */
    struct Sample {
        Ev ev = Ev::BusReset;
        uint8_t epAddr = 0;
        uint8_t result = 0;  // TinyUSB xfer_result_t for XferComplete
        uint8_t speed = 0;   // tusb_speed_t for BusReset
        uint32_t len = 0;
        uint8_t setup[8] = {0};
        int64_t tUs = 0;
    };

    static constexpr size_t kRingDepth = 64;

    /**
     * @brief Enable or disable the per-event ring (counters always run).
     *
     * When full the ring keeps the oldest samples and counts the rest as drops, so the first
     * enumeration survives until a session starts publishing.
     *
     * @param on  true to queue samples; false clears and disables the ring.
     * @note Call before USB.begin() to catch the first enumeration.
     */
    static void setRingEnabled(bool on);

    /**
     * @brief Pop the oldest queued sample.
     * @param[out] out  Filled with the sample on success.
     * @return false if the ring is empty.
     * @note Arduino loop task.
     */
    static bool pop(Sample& out);

    /**
     * @brief Record one DCD event (called by the dcd_event_handler wrap).
     * @param event  TinyUSB `dcd_event_t const*`, passed untyped to keep dcd.h out of this header.
     * @note USB interrupt or task context; IRAM, no allocation, no logging.
     */
    static void onDcdEvent(const void* event);

    /** @brief True once the wrap has delivered at least one event since boot. */
    static bool hookLive() { return events_ != 0; }

    /** @brief Samples not queued because the ring was full. */
    static uint32_t ringDrops() { return ringDrops_; }

    /**
     * @brief Write the since-boot counters as JSON.
     * @param obj  Target object; gets hook_live, bus/setup/descriptor/transfer counters and
     *             the last SETUP.
     */
    static void toJson(JsonObject obj);

    /**
     * @brief Name of a standard request code (USB 2.0 table 9-4).
     * @param bRequest  Standard bRequest.
     * @return Static name, or nullptr if unknown.
     */
    static const char* standardRequestName(uint8_t bRequest);

    /**
     * @brief Name of a descriptor type for GET_DESCRIPTOR wValue high byte.
     * @param type  Descriptor type.
     * @return Static name, or nullptr if unknown.
     */
    static const char* descriptorTypeName(uint8_t type);

private:
    static constexpr size_t kStdRequests = 13;  // GET_STATUS(0) … SYNCH_FRAME(12)

    static uint32_t events_;
    static uint32_t busReset_;
    static uint32_t unplugged_;
    static uint32_t suspend_;
    static uint32_t resume_;
    static uint32_t sof_;
    static uint32_t setup_;
    static uint32_t setupStd_[kStdRequests];
    static uint32_t setupStdOther_;
    static uint32_t setupClass_;
    static uint32_t setupVendor_;
    static uint32_t setupReserved_;
    static uint32_t descDevice_;
    static uint32_t descConfig_;
    static uint32_t descString_;
    static uint32_t descMsOs_;
    static uint32_t descQualifier_;
    static uint32_t descOtherSpeed_;
    static uint32_t descBos_;
    static uint32_t descOther_;
    static uint32_t xferOk_;
    static uint32_t xferFailed_;
    static uint32_t xferStalled_;
    static uint32_t xferEp0_;
    static uint32_t ringDrops_;
    static int64_t lastBusResetUs_;
    static int64_t lastSetupUs_;
    static uint8_t lastSetup_[8];

    static bool ringEnabled_;
    static Sample ring_[kRingDepth];
    static size_t head_;
    static size_t count_;
};
