#pragma once

/**
 * @file AoaTransport.h
 * @brief USB-AOA byte pipe: C1b-0 skeleton (lab) or C1b-P0 observe (aoa_observe boot).
 *
 * Desk role: HU = AOA Host; hu-probe = Device-in-Accessory-Mode after negotiation.
 * Observe bind uses AoaObserveGadget USBVendor bulk — no BCL inside transport.
 *
 * REJECTED: AOA=BCL; parallel MSC+AOA composite; soft-detach as gadget-mode switch.
 */

#include "connected/transport/ITransport.h"

#include <cstddef>
#include <cstdint>

namespace huprobe {
namespace transport {

class AoaTransport : public ITransport {
public:
    static constexpr size_t kRxRingSize = 2048;

    const char* id() const override { return "aoa"; }
    const char* implLevel() const override {
        return observe_bound_ ? "observe" : "skeleton";
    }

    /**
     * @brief Intended USB role for Connected-App capture (desk).
     * @return Always "device_in_accessory_mode" for this probe path.
     */
    const char* usbRoleIntent() const { return "device_in_accessory_mode"; }

    /** @brief True when booted in aoa_observe and USBVendor stack is live. */
    bool usbStackBound() const { return observe_bound_; }

    /**
     * @brief Bind to AoaObserveGadget (call once after gadget begin in aoa_observe boot).
     * Sets impl=observe / usb_stack_bound. No-op if already bound.
     */
    void bindObserveStack();

    bool arm() override;
    void disarm() override;
    bool armed() const override { return armed_; }
    /**
     * @brief True when USBVendor is mounted (or lab link injected).
     * Not an application-peer / Connected-App proof — see usbFunctionMounted().
     */
    bool connected() const override { return connected_; }
    /** @brief Same as connected(); explicit name for status JSON. */
    bool usbFunctionMounted() const { return connected_; }
    /** @brief Always false until a real accessory consumer/FD path is proven. */
    bool applicationPeerProven() const { return false; }

    void poll() override;
    size_t read(uint8_t* dst, size_t maxLen) override;
    /**
     * @brief Skeleton TX accounting, or observe wire TX when wireTxEnabled().
     * Default observe path keeps TX disabled (p0_tx_disabled) until enableWireTx().
     */
    size_t write(const uint8_t* src, size_t len) override;

    /** @brief Gate for B2b Knock-TX on observe stack (off by default). */
    void enableWireTx(bool on) { wire_tx_enabled_ = on; }
    bool wireTxEnabled() const { return wire_tx_enabled_; }

    uint32_t rxBytes() const override { return rx_bytes_; }
    uint32_t txBytes() const override { return tx_bytes_; }

    TransportEvent lastEvent() const override { return last_event_; }
    void clearLastEvent() override { last_event_ = TransportEvent::None; }
    const char* lastError() const override { return last_error_; }

    /**
     * @brief Lab/test only: push bytes as if received on AOA bulk IN.
     * Marks source path as synthetic inject — does not claim NBT wire.
     */
    size_t injectRx(const uint8_t* src, size_t len);

    /** @brief Simulate link up/down without USB (lab / skeleton). */
    void setConnectedForLab(bool up);

    uint32_t injectCount() const { return inject_count_; }
    uint32_t rxOverflow() const { return rx_overflow_; }
    uint32_t bulkRxEvents() const { return bulk_rx_events_; }

private:
    size_t pushRing(const uint8_t* src, size_t len);

    bool observe_bound_ = false;
    bool armed_ = false;
    bool connected_ = false;
    bool lab_link_ = false;
    bool wire_tx_enabled_ = false;
    uint8_t rx_ring_[kRxRingSize];
    size_t rx_head_ = 0;
    size_t rx_tail_ = 0;
    size_t rx_count_ = 0;
    uint32_t rx_bytes_ = 0;
    uint32_t tx_bytes_ = 0;
    uint32_t inject_count_ = 0;
    uint32_t rx_overflow_ = 0;
    uint32_t bulk_rx_events_ = 0;
    TransportEvent last_event_ = TransportEvent::None;
    const char* last_error_ = "";
};

}  // namespace transport
}  // namespace huprobe
