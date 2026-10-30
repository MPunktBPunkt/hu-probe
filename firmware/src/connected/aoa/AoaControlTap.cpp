/**
 * @file AoaControlTap.cpp
 * @brief C1b-P0 / P0.5-A control tap: publish callback SETUPs; optional AOA hypothesis replies.
 *
 * TinyUSB passes CONTROL_STAGE_* into USBVendor.onRequest. Arduino's REQUEST_STAGE_*
 * constants are offset by one (SETUP=0 vs SETUP=1) and must not be used here.
 *
 * After SETUP+sendResponse, DATA/ACK complete callbacks must return true — otherwise
 * TinyUSB stalls EP0 (Lab: 51 Pipe error while 52 worked via DATA early-return).
 */

#include "connected/aoa/AoaControlTap.h"
#include "core/ProbeClock.h"

#include "tusb.h"

#include <cstring>

#if CONFIG_TINYUSB_VENDOR_ENABLED

namespace huprobe {
namespace aoa {

AoaControlTap& AoaControlTap::instance() {
    static AoaControlTap t;
    return t;
}

void AoaControlTap::begin() {
    reset();
}

void AoaControlTap::reset() {
    head_ = tail_ = 0;
    setup_count_ = vendor_setup_count_ = aoa51_ = aoa52_ = aoa53_ = aoa58_ = 0;
    aoa51_respond_ok_ = drops_ = 0;
    string_scratch_len_ = 0;
    pending_string_index_ = 0xff;
    respond_in_flight_ = false;
    std::memset(string_scratch_, 0, sizeof(string_scratch_));
}

bool AoaControlTap::push(const AoaControlSample& s) {
    const uint32_t next = (head_ + 1) % kQueueDepth;
    if (next == tail_) {
        drops_++;
        return false;
    }
    q_[head_] = s;
    head_ = next;
    return true;
}

bool AoaControlTap::pop(AoaControlSample& out) {
    if (tail_ == head_) return false;
    out = q_[tail_];
    tail_ = (tail_ + 1) % kQueueDepth;
    return true;
}

bool AoaControlTap::onControlRequest(uint8_t rhport, uint8_t stage,
                                     arduino_usb_control_request_t const* request,
                                     USBVendor* vendor) {
    if (!request) return false;

    const uint8_t bmRequestType = static_cast<uint8_t>(
        (request->bmRequestDirection << 7) | (request->bmRequestType << 5) |
        request->bmRequestRecipient);
    const uint8_t bRequest = request->bRequest;
    const uint16_t wValue = request->wValue;
    const uint16_t wIndex = request->wIndex;
    const uint16_t wLength = request->wLength;
    const bool vendor_type = ((bmRequestType >> 5) & 0x3) == 2;

    AoaControlSample s;
    s.bm_request_type = bmRequestType;
    s.b_request = bRequest;
    s.w_value = wValue;
    s.w_index = wIndex;
    s.w_length = wLength;
    s.stage = stage;
    s.t_us = ProbeClock::nowUs();
    s.hypothesis_handled = false;
    s.str[0] = '\0';
    s.str_len = 0;

    // DATA/ACK after a successful SETUP respond: must not stall EP0.
    if (stage != CONTROL_STAGE_SETUP && respond_in_flight_) {
        if (vendor_type && bRequest == 52 && stage == CONTROL_STAGE_DATA &&
            string_scratch_len_ > 0) {
            const size_t n =
                string_scratch_len_ < sizeof(s.str) - 1 ? string_scratch_len_ : sizeof(s.str) - 1;
            std::memcpy(s.str, string_scratch_, n);
            s.str[n] = '\0';
            s.str_len = static_cast<uint8_t>(n);
            s.w_index = pending_string_index_;
            push(s);
        }
        if (stage == CONTROL_STAGE_ACK) {
            respond_in_flight_ = false;
        }
        return true;
    }

    // Observe always: count/publish every SETUP that reaches this callback (not full EP0).
    if (stage == CONTROL_STAGE_SETUP) {
        setup_count_++;
        if (vendor_type) {
            vendor_setup_count_++;
            if (bRequest == 51) aoa51_++;
            else if (bRequest == 52) aoa52_++;
            else if (bRequest == 53) aoa53_++;
            else if (bRequest == 58) aoa58_++;
        }
    }

    bool handled = false;

    // Respond path is optional and separate from observe counters above.
    // Pass the original request into sendResponse (TinyUSB copies it).
    if (hypothesis_respond_ && vendor && vendor_type && stage == CONTROL_STAGE_SETUP) {
        // Desk hypothesis AOA device-side replies (AOSP). Not NBT FACT.
        if (bRequest == 51 && (bmRequestType & 0x80)) {
            handled = vendor->sendResponse(rhport, request, &aoa_protocol_le_, 2);
            s.hypothesis_handled = handled;
            if (handled) aoa51_respond_ok_++;
        } else if (bRequest == 52 && !(bmRequestType & 0x80)) {
            pending_string_index_ = static_cast<uint8_t>(wIndex & 0xff);
            string_scratch_len_ = wLength > sizeof(string_scratch_) ? sizeof(string_scratch_)
                                                                   : wLength;
            std::memset(string_scratch_, 0, sizeof(string_scratch_));
            if (string_scratch_len_ == 0) {
                handled = vendor->sendResponse(rhport, request, nullptr, 0);
            } else {
                handled =
                    vendor->sendResponse(rhport, request, string_scratch_, string_scratch_len_);
            }
            s.hypothesis_handled = handled;
        } else if (bRequest == 53 && !(bmRequestType & 0x80)) {
            // START accepted; post-53 VID re-enum NOT IMPLEMENTED (Arduino lock).
            handled = vendor->sendResponse(rhport, request, nullptr, 0);
            s.hypothesis_handled = handled;
        } else if (bRequest == 58 && !(bmRequestType & 0x80)) {
            handled = vendor->sendResponse(rhport, request, nullptr, 0);
            s.hypothesis_handled = handled;
        }
        respond_in_flight_ = handled;
    }

    if (stage == CONTROL_STAGE_SETUP) {
        push(s);
    }
    return handled;
}

}  // namespace aoa
}  // namespace huprobe

#else

namespace huprobe {
namespace aoa {

AoaControlTap& AoaControlTap::instance() {
    static AoaControlTap t;
    return t;
}
void AoaControlTap::begin() { reset(); }
void AoaControlTap::reset() {}
bool AoaControlTap::onControlRequest(uint8_t, uint8_t, void const*, void*) { return false; }
bool AoaControlTap::pop(AoaControlSample&) { return false; }

}  // namespace aoa
}  // namespace huprobe

#endif  // CONFIG_TINYUSB_VENDOR_ENABLED
