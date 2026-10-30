/**
 * @file AoaObserveGadget.cpp
 * @brief USBVendor-only gadget for C1b-P0 aoa_observe boot mode.
 */

#include "connected/aoa/AoaObserveGadget.h"
#include "connected/aoa/AoaControlTap.h"
#include "core/DiagLog.h"

#include "USB.h"
#include "USBVendor.h"
#include "esp32-hal-tinyusb.h"

#if CONFIG_TINYUSB_VENDOR_ENABLED

namespace huprobe {
namespace aoa {

static USBVendor g_vendor(64);
static AoaObserveGadget* g_self = nullptr;

static bool onVendorControl(uint8_t rhport, uint8_t stage,
                            arduino_usb_control_request_t const* request) {
    if (!request) return false;
    const uint8_t bm = static_cast<uint8_t>(
        (request->bmRequestDirection << 7) | (request->bmRequestType << 5) |
        request->bmRequestRecipient);
    return AoaControlTap::instance().onControlRequest(
        rhport, stage, bm, request->bRequest, request->wValue, request->wIndex,
        request->wLength, &g_vendor);
}

static void onUsbEvent(void* /*arg*/, esp_event_base_t /*base*/, int32_t id, void* /*data*/) {
    if (!g_self) return;
    if (id == ARDUINO_USB_STARTED_EVENT) {
        g_self->notifyUsbStarted();
    } else if (id == ARDUINO_USB_STOPPED_EVENT) {
        g_self->notifyUsbStopped();
    }
}

AoaObserveGadget& AoaObserveGadget::instance() {
    static AoaObserveGadget g;
    return g;
}

void AoaObserveGadget::notifyUsbStarted() {
    plugged_ = true;
    attach_count_++;
    transport_epoch_++;
    DiagLog::printf("[AOA] USB STARTED (P0-A attach hypothesis path)\n");
}

void AoaObserveGadget::notifyUsbStopped() {
    plugged_ = false;
    DiagLog::printf("[AOA] USB STOPPED\n");
}

void AoaObserveGadget::begin(Session* session, bool startDetached, AoaIdentityProfile profile) {
    session_ = session;
    profile_ = profile;
    g_self = this;
    AoaControlTap::instance().begin();
    // Accessory observe = passive. PhoneLike enables desk AOA 51/52/53 responder (Lab A / B1).
    AoaControlTap::instance().setHypothesisRespond(profile_ == AoaIdentityProfile::PhoneLike);

    if (profile_ == AoaIdentityProfile::PhoneLike) {
        // B1 desk hypothesis: non-accessory IDs so HU may run classic 51→52→53.
        // Samsung-ish VID/PID + Android strings — NOT NBT FACT; observation only.
        vid_ = 0x04E8;
        pid_ = 0x6860;
        identity_note_ = "hypothesis_phone_like_0x04E8_0x6860_b1";
        USB.VID(vid_);
        USB.PID(pid_);
        USB.manufacturerName("Android");
        USB.productName("AOA Phone-like");
        USB.serialNumber("HUPROBE-AOA1");
    } else {
        // Desk HYPOTHESIS: already-in-accessory-mode Google VID/PID. Not NBT FACT.
        vid_ = 0x18D1;
        pid_ = 0x2D00;
        identity_note_ = "hypothesis_google_accessory_0x18D1_0x2D00";
        USB.VID(vid_);
        USB.PID(pid_);
        USB.manufacturerName("Google");
        USB.productName("AOA Observe");
        USB.serialNumber("HUPROBE-AOA0");
    }
    USB.usbClass(0xFF);
    USB.usbSubClass(0xFF);
    USB.usbProtocol(0x00);

    USB.onEvent(onUsbEvent);
    g_vendor.onRequest(onVendorControl);
    g_vendor.begin();
    if (!USB.begin()) {
        DiagLog::printf("[AOA] USB.begin failed\n");
        ready_ = false;
        return;
    }
    ready_ = true;
    soft_attached_ = true;
    if (startDetached && tud_disconnect()) soft_attached_ = false;
    DiagLog::printf(
        "[AOA] observe ready profile=%s vid=0x%04X pid=0x%04X (%s) soft=%d hypothesis_respond=%d\n",
        profile_ == AoaIdentityProfile::PhoneLike ? "phone_like" : "accessory", (unsigned)vid_,
        (unsigned)pid_, identityNote(), soft_attached_ ? 1 : 0,
        AoaControlTap::instance().hypothesisRespond() ? 1 : 0);
}

void AoaObserveGadget::loop() {
    if (!ready_) return;
    if (soft_reattach_at_ms_ && millis() >= soft_reattach_at_ms_) {
        soft_reattach_at_ms_ = 0;
        softAttach();
    }
}

bool AoaObserveGadget::vendorMounted() const {
    return ready_ && g_vendor.mounted();
}

size_t AoaObserveGadget::drainVendorRx(uint8_t* dst, size_t maxLen) {
    if (!ready_ || !dst || maxLen == 0) return 0;
    return g_vendor.read(dst, maxLen);
}

size_t AoaObserveGadget::writeVendor(const uint8_t* src, size_t len) {
    if (!ready_ || !src || len == 0) return 0;
    if (!g_vendor.mounted()) return 0;
    return g_vendor.write(src, len);
}

bool AoaObserveGadget::softDetach(uint16_t holdMs) {
    if (!ready_) return false;
    soft_reattach_at_ms_ = 0;
    if (!tud_disconnect()) return false;
    soft_attached_ = false;
    if (holdMs) {
        if (holdMs > 5000) holdMs = 5000;
        soft_reattach_at_ms_ = millis() + holdMs;
    }
    return true;
}

bool AoaObserveGadget::softAttach() {
    if (!ready_) return false;
    if (!tud_connect()) return false;
    soft_attached_ = true;
    transport_epoch_++;
    return true;
}

bool AoaObserveGadget::softCycle(uint16_t holdMs) {
    if (holdMs < 50) holdMs = 50;
    if (holdMs > 5000) holdMs = 5000;
    return softDetach(holdMs);
}

}  // namespace aoa
}  // namespace huprobe

#else

namespace huprobe {
namespace aoa {

AoaObserveGadget& AoaObserveGadget::instance() {
    static AoaObserveGadget g;
    return g;
}
void AoaObserveGadget::begin(Session*, bool, AoaIdentityProfile) {}
void AoaObserveGadget::loop() {}
void AoaObserveGadget::notifyUsbStarted() {}
void AoaObserveGadget::notifyUsbStopped() {}
bool AoaObserveGadget::softDetach(uint16_t) { return false; }
bool AoaObserveGadget::softAttach() { return false; }
bool AoaObserveGadget::softCycle(uint16_t) { return false; }
size_t AoaObserveGadget::drainVendorRx(uint8_t*, size_t) { return 0; }
size_t AoaObserveGadget::writeVendor(const uint8_t*, size_t) { return 0; }
bool AoaObserveGadget::vendorMounted() const { return false; }

}  // namespace aoa
}  // namespace huprobe

#endif
