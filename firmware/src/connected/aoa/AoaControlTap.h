#pragma once

/**
 * @file AoaControlTap.h
 * @brief Queue USB vendor control SETUPs for C1b-P0 AOA observe (USB task → loop).
 *
 * Desk hypothesis (NOT NBT FACT until captured): AOA GET_PROTOCOL=51, SEND_STRING=52,
 * START=53. Responses are optional behind hypothesisRespond().
 */

#include <cstddef>
#include <cstdint>

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

    /** @brief When true, answer desk-hypothesis AOA 51/52/53 on SETUP. */
    void setHypothesisRespond(bool on) { hypothesis_respond_ = on; }
    bool hypothesisRespond() const { return hypothesis_respond_; }

    /**
     * @brief USB-task callback: record SETUP and optionally answer AOA hypothesis.
     * @return true if TinyUSB should consider the request handled.
     */
    bool onControlRequest(uint8_t rhport, uint8_t stage, uint8_t bmRequestType,
                          uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                          uint16_t wLength, void* vendorSendResponseCtx);

    /** @brief Pop one sample for EventBus publish (Arduino loop). */
    bool pop(AoaControlSample& out);

    uint32_t controlCount() const { return control_count_; }
    uint32_t aoa51Count() const { return aoa51_; }
    uint32_t aoa52Count() const { return aoa52_; }
    uint32_t aoa53Count() const { return aoa53_; }
    uint32_t dropCount() const { return drops_; }
    bool sawAoaControl() const { return aoa51_ || aoa52_ || aoa53_; }

private:
    AoaControlTap() = default;

    bool push(const AoaControlSample& s);

    volatile uint32_t head_ = 0;
    volatile uint32_t tail_ = 0;
    AoaControlSample q_[kQueueDepth];
    uint32_t control_count_ = 0;
    uint32_t aoa51_ = 0;
    uint32_t aoa52_ = 0;
    uint32_t aoa53_ = 0;
    uint32_t drops_ = 0;
    bool hypothesis_respond_ = true;
    uint16_t aoa_protocol_le_ = 1;  // AOA 1.0 LE
    uint8_t string_scratch_[256] = {0};
    uint16_t string_scratch_len_ = 0;
    uint8_t pending_string_index_ = 0xff;
};

}  // namespace aoa
}  // namespace huprobe
