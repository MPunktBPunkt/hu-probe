#pragma once

/**
 * @file AoaObserveGadget.h
 * @brief C1b-P0/P0.5 USBVendor-only gadget for AOA observe boots (no MSC interface).
 *
 * Profiles (NVS usb_gadget + reboot):
 * - Accessory (`aoa_observe`): VID/PID 0x18D1:0x2D00 — observe-only (hypothesis_respond=false)
 * - PhoneLike (`aoa_phone_like`): non-accessory IDs + desk 51/52/53 responder (Lab A / B1)
 *
 * Soft-detach is D+ only — not a gadget-mode or identity switch.
 */

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include "session/Session.h"

namespace huprobe {
namespace aoa {

/** @brief USB identity presented at USB.begin() (boot-fixed). */
enum class AoaIdentityProfile : uint8_t {
    Accessory = 0,  ///< Google accessory 0x18D1:0x2D00
    PhoneLike = 1,  ///< Non-accessory IDs for B1 reachability
};

class AoaObserveGadget {
public:
    static AoaObserveGadget& instance();

    /**
     * @brief Enable USBVendor, set identity, USB.begin().
     * @param session  Optional session for markers; may be nullptr.
     * @param startDetached  Soft D+ off after begin (lab WiFi-first).
     * @param profile  Accessory vs PhoneLike (B1).
     */
    void begin(Session* session = nullptr, bool startDetached = false,
               AoaIdentityProfile profile = AoaIdentityProfile::Accessory);

    void loop();

    bool ready() const { return ready_; }
    bool plugged() const { return plugged_; }
    bool softAttached() const { return soft_attached_; }
    bool usbStackBound() const { return ready_; }
    AoaIdentityProfile profile() const { return profile_; }

    /** @brief P0-0: MSC must never be present in this gadget. */
    bool mscInterfacePresent() const { return false; }
    bool vendorInterfacePresent() const { return ready_; }

    uint16_t vid() const { return vid_; }
    uint16_t pid() const { return pid_; }
    const char* identityNote() const { return identity_note_; }

    bool softDetach(uint16_t holdMs = 0);
    bool softAttach();
    bool softCycle(uint16_t holdMs = 600);

    /** @brief Drain USBVendor RX into AoaTransport ring (called by transport poll). */
    size_t drainVendorRx(uint8_t* dst, size_t maxLen);

    /**
     * @brief Bulk OUT via USBVendor (B2b knock / gated TX). Observe path only.
     * @return Bytes accepted by TinyUSB, or 0 on error.
     */
    size_t writeVendor(const uint8_t* src, size_t len);

    bool vendorMounted() const;

    uint32_t attachCount() const { return attach_count_; }
    uint32_t transportEpoch() const { return transport_epoch_; }

    /** @brief USB event path (STARTED/STOPPED) — called from Arduino USB callback. */
    void notifyUsbStarted();
    void notifyUsbStopped();

private:
    AoaObserveGadget() = default;

    Session* session_ = nullptr;
    bool ready_ = false;
    bool plugged_ = false;
    bool soft_attached_ = false;
    AoaIdentityProfile profile_ = AoaIdentityProfile::Accessory;
    uint16_t vid_ = 0x18D1;
    uint16_t pid_ = 0x2D00;
    const char* identity_note_ = "hypothesis_google_accessory_0x18D1_0x2D00";
    uint32_t attach_count_ = 0;
    uint32_t transport_epoch_ = 0;
    uint32_t soft_reattach_at_ms_ = 0;
};

}  // namespace aoa
}  // namespace huprobe
