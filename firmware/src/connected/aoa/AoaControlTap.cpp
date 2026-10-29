/**
 * @file AoaControlTap.cpp
 * @brief C1b-P0 / P0.5-A control tap: publish callback SETUPs; optional AOA hypothesis replies.
 */

#include "connected/aoa/AoaControlTap.h"
#include "core/ProbeClock.h"

#include "USBVendor.h"

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
    setup_count_ = vendor_setup_count_ = aoa51_ = aoa52_ = aoa53_ = drops_ = 0;
    string_scratch_len_ = 0;
    pending_string_index_ = 0xff;
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

bool AoaControlTap::onControlRequest(uint8_t rhport, uint8_t stage, uint8_t bmRequestType,
                                     uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                     uint16_t wLength, void* vendorSendResponseCtx) {
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

    const bool vendor = ((bmRequestType >> 5) & 0x3) == 2;

    // P0.5-A: count/publish every SETUP that reaches this callback (still not full EP0).
    if (stage == REQUEST_STAGE_SETUP) {
        setup_count_++;
        if (vendor) {
            vendor_setup_count_++;
            if (bRequest == 51) aoa51_++;
            else if (bRequest == 52) aoa52_++;
            else if (bRequest == 53) aoa53_++;
        }
    }

    // After OUT data stage of SEND_STRING: publish captured identity (audit B3).
    if (vendor && bRequest == 52 && stage == REQUEST_STAGE_DATA && string_scratch_len_ > 0) {
        const size_t n =
            string_scratch_len_ < sizeof(s.str) - 1 ? string_scratch_len_ : sizeof(s.str) - 1;
        std::memcpy(s.str, string_scratch_, n);
        s.str[n] = '\0';
        s.str_len = static_cast<uint8_t>(n);
        s.w_index = pending_string_index_;
        push(s);
        return true;
    }

    USBVendor* v = static_cast<USBVendor*>(vendorSendResponseCtx);
    bool handled = false;

    if (hypothesis_respond_ && v && vendor && stage == REQUEST_STAGE_SETUP) {
        // Desk hypothesis AOA device-side replies (AOSP). Not NBT FACT.
        // EVID-AOA-2026-10-28-002..004
        arduino_usb_control_request_t req;
        req.bmRequestRecipient = bmRequestType & 0x1f;
        req.bmRequestType = (bmRequestType >> 5) & 0x3;
        req.bmRequestDirection = (bmRequestType >> 7) & 0x1;
        req.bRequest = bRequest;
        req.wValue = wValue;
        req.wIndex = wIndex;
        req.wLength = wLength;

        if (bRequest == 51 && (bmRequestType & 0x80)) {
            handled = v->sendResponse(rhport, &req, &aoa_protocol_le_, 2);
            s.hypothesis_handled = handled;
        } else if (bRequest == 52 && !(bmRequestType & 0x80)) {
            pending_string_index_ = static_cast<uint8_t>(wIndex & 0xff);
            string_scratch_len_ = wLength > sizeof(string_scratch_) ? sizeof(string_scratch_)
                                                                   : wLength;
            std::memset(string_scratch_, 0, sizeof(string_scratch_));
            if (string_scratch_len_ == 0) {
                handled = v->sendResponse(rhport, &req, nullptr, 0);
            } else {
                handled =
                    v->sendResponse(rhport, &req, string_scratch_, string_scratch_len_);
            }
            s.hypothesis_handled = handled;
        } else if (bRequest == 53 && !(bmRequestType & 0x80)) {
            // START accepted; post-53 VID re-enum NOT IMPLEMENTED (Arduino lock).
            handled = v->sendResponse(rhport, &req, nullptr, 0);
            s.hypothesis_handled = handled;
        } else if (bRequest == 58 && !(bmRequestType & 0x80)) {
            // AOAv2 SET_AUDIO_MODE — accept/ignore (audio deprecated); still log via push.
            handled = v->sendResponse(rhport, &req, nullptr, 0);
            s.hypothesis_handled = handled;
        }
    }

    if (stage == REQUEST_STAGE_SETUP) {
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
bool AoaControlTap::onControlRequest(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t, uint16_t,
                                     uint16_t, void*) {
    return false;
}
bool AoaControlTap::pop(AoaControlSample&) { return false; }

}  // namespace aoa
}  // namespace huprobe

#endif  // CONFIG_TINYUSB_VENDOR_ENABLED
