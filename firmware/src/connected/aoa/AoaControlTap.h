#pragma once

/**
 * @file AoaControlTap.h
 * @brief Queue USB control SETUPs that reach USBVendor.onRequest (C1b-P0 / P0.5-A).
 *
 * Publishes every SETUP that arrives on this callback (not a full EP0 sniffer).
 * AOA GET_PROTOCOL=51 / SEND_STRING=52 / START=53 are counted when vendor.
 *
 * Stage values are TinyUSB `CONTROL_STAGE_*` (SETUP=1), not Arduino
 * `REQUEST_STAGE_*` (SETUP=0). USBVendor forwards the TinyUSB stage unchanged.
 *
 * After a successful SETUP respond, DATA/ACK must return true — TinyUSB stalls
 * EP0 if the complete callback returns false (seen as host Pipe error on 51).
 *
 * Desk hypothesis responses are optional behind hypothesisRespond() and are
 * independent of observe counters / SETUP publish.
 */

#include <cstddef>
#include <cstdint>

#include "USBVendor.h"

namespace huprobe {
namespace aoa {

/** @brief One captured control SETUP/DATA sample (USB task → loop). */
struct AoaControlSample {
    uint8_t bm_request_type = 0;
    uint8_t b_request = 0;
    uint16_t w_value = 0;
    uint16_t w_index = 0;
    uint16_t w_length = 0;
    uint8_t stage = 0;
    /**
     * @brief True if sendResponse() was called (device queued a reply).
     * Not proof the host accepted it — prefer JSON key device_response_queued.
     */
    bool hypothesis_handled = false;
    int64_t t_us = 0;
    /** @brief For req 52: captured UTF-8 identity string (desk B3), truncated. */
    char str[48] = {0};
    uint8_t str_len = 0;
};

/**
 * @brief Lock-free-ish ring of control samples + optional AOA hypothesis replies.
 */
class AoaControlTap {
public:
    static constexpr size_t kQueueDepth = 32;

    static AoaControlTap& instance();

    void begin();
    void reset();

    /** @brief When true, answer desk-hypothesis AOA 51/52/53/58 on SETUP. */
    void setHypothesisRespond(bool on) { hypothesis_respond_ = on; }
    bool hypothesisRespond() const { return hypothesis_respond_; }

    /**
     * @brief USB-task callback: record SETUP and optionally answer AOA hypothesis.
     * @param request  Original control request (needed by tud_control_xfer).
     * @return true if TinyUSB should consider the request handled.
     */
#if CONFIG_TINYUSB_VENDOR_ENABLED
    bool onControlRequest(uint8_t rhport, uint8_t stage,
                          arduino_usb_control_request_t const* request, USBVendor* vendor);
#else
    bool onControlRequest(uint8_t rhport, uint8_t stage, void const* request, void* vendor) {
        (void)rhport;
        (void)stage;
        (void)request;
        (void)vendor;
        return false;
    }
#endif

    /** @brief Pop one sample for EventBus publish (Arduino loop). */
    bool pop(AoaControlSample& out);

    /** @brief All CONTROL_STAGE_SETUP samples that reached this callback. */
    uint32_t setupCount() const { return setup_count_; }
    /** @brief Vendor-type SETUPs only (type bits == 2). */
    uint32_t vendorSetupCount() const { return vendor_setup_count_; }
    /**
     * @brief Legacy alias of vendorSetupCount().
     * Historically named control_count; always meant Vendor-SETUPs only.
     */
    uint32_t controlCount() const { return vendor_setup_count_; }
    uint32_t aoa51Count() const { return aoa51_; }
    uint32_t aoa52Count() const { return aoa52_; }
    uint32_t aoa53Count() const { return aoa53_; }
    uint32_t aoa58Count() const { return aoa58_; }
    uint32_t dropCount() const { return drops_; }
    /** @brief Successful sendResponse() calls for GET_PROTOCOL (51). */
    uint32_t aoa51RespondOk() const { return aoa51_respond_ok_; }
    /** @brief True when the control ring has not overflowed since reset. */
    bool captureValid() const { return drops_ == 0; }
    bool sawAoaControl() const { return aoa51_ || aoa52_ || aoa53_ || aoa58_; }

private:
    AoaControlTap() = default;

    bool push(const AoaControlSample& s);

    volatile uint32_t head_ = 0;
    volatile uint32_t tail_ = 0;
    AoaControlSample q_[kQueueDepth];
    uint32_t setup_count_ = 0;
    uint32_t vendor_setup_count_ = 0;
    uint32_t aoa51_ = 0;
    uint32_t aoa52_ = 0;
    uint32_t aoa53_ = 0;
    uint32_t aoa58_ = 0;
    uint32_t aoa51_respond_ok_ = 0;
    uint32_t drops_ = 0;
    /** @brief Default passive; PhoneLike gadget enables responder at begin(). */
    bool hypothesis_respond_ = false;
    /**
     * @brief SETUP respond succeeded — DATA/ACK complete_cb must return true
     * or TinyUSB stalls EP0 (host Pipe error).
     */
    bool respond_in_flight_ = false;
    uint16_t aoa_protocol_le_ = 1;  // AOA 1.0 LE
    uint8_t string_scratch_[256] = {0};
    uint16_t string_scratch_len_ = 0;
    uint8_t pending_string_index_ = 0xff;
};

}  // namespace aoa
}  // namespace huprobe
