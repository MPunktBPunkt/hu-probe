/**
 * @file AoaTransport.cpp
 * @brief C1b-0 skeleton + C1b-P0 observe drain from AoaObserveGadget.
 */

#include "connected/transport/AoaTransport.h"
#include "connected/aoa/AoaObserveGadget.h"

#include <cstring>

namespace huprobe {
namespace transport {

void AoaTransport::bindObserveStack() {
    observe_bound_ = true;
}

bool AoaTransport::arm() {
    armed_ = true;
    connected_ = false;
    lab_link_ = false;
    rx_head_ = rx_tail_ = rx_count_ = 0;
    last_error_ = "";
    last_event_ = TransportEvent::None;
    if (observe_bound_) {
        using huprobe::aoa::AoaObserveGadget;
        if (AoaObserveGadget::instance().vendorMounted()) {
            connected_ = true;
            last_event_ = TransportEvent::Connected;
        }
    }
    return true;
}

void AoaTransport::disarm() {
    if (connected_) {
        connected_ = false;
        last_event_ = TransportEvent::Disconnected;
    } else {
        last_event_ = TransportEvent::None;
    }
    armed_ = false;
    lab_link_ = false;
    rx_head_ = rx_tail_ = rx_count_ = 0;
    last_error_ = "";
}

void AoaTransport::poll() {
    if (!armed_ || !observe_bound_) return;
    using huprobe::aoa::AoaObserveGadget;
    AoaObserveGadget& g = AoaObserveGadget::instance();
    const bool mounted = g.vendorMounted();
    if (mounted && !connected_ && !lab_link_) {
        connected_ = true;
        last_event_ = TransportEvent::Connected;
    } else if (!mounted && connected_ && !lab_link_) {
        connected_ = false;
        last_event_ = TransportEvent::Disconnected;
    }
    uint8_t tmp[64];
    for (;;) {
        const size_t n = g.drainVendorRx(tmp, sizeof(tmp));
        if (n == 0) break;
        const size_t accepted = pushRing(tmp, n);
        if (accepted > 0) {
            bulk_rx_events_++;
            if (last_event_ != TransportEvent::Connected) {
                last_event_ = TransportEvent::RxBytes;
            }
        }
    }
}

size_t AoaTransport::pushRing(const uint8_t* src, size_t len) {
    if (!src || len == 0) return 0;
    size_t accepted = 0;
    for (size_t i = 0; i < len; ++i) {
        if (rx_count_ >= kRxRingSize) {
            rx_overflow_++;
            last_error_ = "rx_ring_overflow";
            last_event_ = TransportEvent::TransportError;
            break;
        }
        rx_ring_[rx_tail_] = src[i];
        rx_tail_ = (rx_tail_ + 1) % kRxRingSize;
        rx_count_++;
        accepted++;
    }
    return accepted;
}

size_t AoaTransport::read(uint8_t* dst, size_t maxLen) {
    if (!dst || maxLen == 0 || rx_count_ == 0) return 0;
    size_t n = 0;
    while (n < maxLen && rx_count_ > 0) {
        dst[n++] = rx_ring_[rx_head_];
        rx_head_ = (rx_head_ + 1) % kRxRingSize;
        rx_count_--;
    }
    if (n > 0) {
        rx_bytes_ += static_cast<uint32_t>(n);
        last_event_ = TransportEvent::RxBytes;
    }
    return n;
}

size_t AoaTransport::write(const uint8_t* src, size_t len) {
    if (!armed_) {
        last_error_ = "not_armed";
        last_event_ = TransportEvent::TransportError;
        return 0;
    }
    // C1b-P0 default: no wire TX. B2b knock enables wire_tx_enabled_ explicitly.
    if (observe_bound_ && !wire_tx_enabled_) {
        last_error_ = "p0_tx_disabled";
        last_event_ = TransportEvent::TransportError;
        return 0;
    }
    if (!connected_ && !(observe_bound_ && wire_tx_enabled_)) {
        last_error_ = "not_connected";
        last_event_ = TransportEvent::TransportError;
        return 0;
    }
    if (!src && len > 0) {
        last_error_ = "null_src";
        last_event_ = TransportEvent::TransportError;
        return 0;
    }
    if (observe_bound_ && wire_tx_enabled_) {
        using huprobe::aoa::AoaObserveGadget;
        if (!AoaObserveGadget::instance().vendorMounted()) {
            last_error_ = "vendor_not_mounted";
            last_event_ = TransportEvent::TransportError;
            return 0;
        }
        const size_t n = AoaObserveGadget::instance().writeVendor(src, len);
        if (n == 0) {
            last_error_ = "vendor_write_zero";
            last_event_ = TransportEvent::TransportError;
            return 0;
        }
        tx_bytes_ += static_cast<uint32_t>(n);
        last_event_ = TransportEvent::TxBytes;
        last_error_ = "";
        return n;
    }
    tx_bytes_ += static_cast<uint32_t>(len);
    last_event_ = TransportEvent::TxBytes;
    last_error_ = "";
    return len;
}

size_t AoaTransport::injectRx(const uint8_t* src, size_t len) {
    if (!armed_) {
        last_error_ = "not_armed";
        last_event_ = TransportEvent::TransportError;
        return 0;
    }
    if (!src || len == 0) return 0;
    const size_t accepted = pushRing(src, len);
    if (accepted > 0) {
        inject_count_++;
        if (!connected_) {
            connected_ = true;
            lab_link_ = true;
            last_event_ = TransportEvent::Connected;
        } else {
            last_event_ = TransportEvent::RxBytes;
        }
        last_error_ = "";
    }
    return accepted;
}

void AoaTransport::setConnectedForLab(bool up) {
    if (!armed_) return;
    if (up && !connected_) {
        connected_ = true;
        lab_link_ = true;
        last_event_ = TransportEvent::Connected;
        last_error_ = "";
    } else if (!up && connected_) {
        connected_ = false;
        lab_link_ = false;
        last_event_ = TransportEvent::Disconnected;
    }
}

}  // namespace transport
}  // namespace huprobe
