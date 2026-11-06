#pragma once

/**
 * @file UsbSessionTimeline.h
 * @brief Compact monotonic USB session timeline (ring, host-testable).
 *
 * Correlates with Analyzer host-behaviour sessions without duplicating a CBW ring.
 * Overflow increments overflow_count; oldest events are dropped.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace huprobe {

enum class UsbTlEvent : uint8_t {
    SessionStart = 0,
    SessionStop,
    Attach,
    Detach,
    VendorMounted,
    Setup,
    Aoa51,
    Aoa52,
    Aoa53,
    BulkRx,
    SoftDetach,
    SoftAttach,
    ScsiOpcode,  ///< detail = opcode byte (MSC path)
    Unknown = 255
};

inline const char* usbTlEventName(UsbTlEvent e) {
    switch (e) {
        case UsbTlEvent::SessionStart: return "session_start";
        case UsbTlEvent::SessionStop: return "session_stop";
        case UsbTlEvent::Attach: return "attach";
        case UsbTlEvent::Detach: return "detach";
        case UsbTlEvent::VendorMounted: return "vendor_mounted";
        case UsbTlEvent::Setup: return "setup";
        case UsbTlEvent::Aoa51: return "aoa_51";
        case UsbTlEvent::Aoa52: return "aoa_52";
        case UsbTlEvent::Aoa53: return "aoa_53";
        case UsbTlEvent::BulkRx: return "bulk_rx";
        case UsbTlEvent::SoftDetach: return "soft_detach";
        case UsbTlEvent::SoftAttach: return "soft_attach";
        case UsbTlEvent::ScsiOpcode: return "scsi_opcode";
        default: return "unknown";
    }
}

struct UsbTlEntry {
    uint32_t t_ms = 0;
    UsbTlEvent type = UsbTlEvent::Unknown;
    uint32_t detail = 0;
};

/**
 * @brief Fixed ring of timeline events with monotonic timestamps.
 *
 * push() ignores empty sessions until beginSession(); reset clears all state.
 */
class UsbSessionTimeline {
public:
    static constexpr size_t kCapacity = 64;

    void reset() {
        head_ = size_ = 0;
        overflow_count_ = 0;
        session_active_ = false;
        session_id_[0] = '\0';
        t0_ms_ = 0;
        last_ms_ = 0;
        has_t0_ = false;
    }

    void beginSession(const char* sid, uint32_t t_ms) {
        reset();
        session_active_ = true;
        if (sid) {
            std::strncpy(session_id_, sid, sizeof(session_id_) - 1);
            session_id_[sizeof(session_id_) - 1] = '\0';
        }
        t0_ms_ = t_ms;
        has_t0_ = true;
        push(UsbTlEvent::SessionStart, t_ms, 0);
    }

    void endSession(uint32_t t_ms) {
        if (!session_active_) return;
        push(UsbTlEvent::SessionStop, t_ms, 0);
        session_active_ = false;
    }

    void push(UsbTlEvent type, uint32_t t_ms, uint32_t detail = 0) {
        if (!session_active_ && type != UsbTlEvent::SessionStart) return;
        if (!has_t0_) {
            t0_ms_ = t_ms;
            has_t0_ = true;
        }
        if (t_ms < last_ms_) t_ms = last_ms_;  // monotonic clamp
        last_ms_ = t_ms;

        if (size_ == kCapacity) {
            // Drop oldest.
            head_ = (head_ + 1) % kCapacity;
            --size_;
            ++overflow_count_;
        }
        const size_t idx = (head_ + size_) % kCapacity;
        entries_[idx].t_ms = t_ms;
        entries_[idx].type = type;
        entries_[idx].detail = detail;
        ++size_;
    }

    bool active() const { return session_active_; }
    const char* sessionId() const { return session_id_; }
    size_t size() const { return size_; }
    uint32_t overflowCount() const { return overflow_count_; }
    uint32_t firstEventMs() const { return has_t0_ ? t0_ms_ : 0; }
    uint32_t lastEventMs() const { return has_t0_ ? last_ms_ : 0; }
    uint32_t durationMs() const {
        return has_t0_ && last_ms_ >= t0_ms_ ? (last_ms_ - t0_ms_) : 0;
    }
    bool hasTiming() const { return has_t0_; }

    /** @brief Indexed access oldest→newest; returns false if i >= size(). */
    bool at(size_t i, UsbTlEntry& out) const {
        if (i >= size_) return false;
        out = entries_[(head_ + i) % kCapacity];
        return true;
    }

    /** @brief Relative T+ offset from session start. */
    uint32_t relativeMs(uint32_t t_ms) const {
        if (!has_t0_ || t_ms < t0_ms_) return 0;
        return t_ms - t0_ms_;
    }

private:
    UsbTlEntry entries_[kCapacity];
    size_t head_ = 0;
    size_t size_ = 0;
    uint32_t overflow_count_ = 0;
    bool session_active_ = false;
    bool has_t0_ = false;
    uint32_t t0_ms_ = 0;
    uint32_t last_ms_ = 0;
    char session_id_[48] = {0};
};

}  // namespace huprobe
