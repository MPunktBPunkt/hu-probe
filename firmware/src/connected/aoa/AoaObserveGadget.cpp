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

void AoaObserveGadget::begin(Session* session, bool startDetached) {
    session_ = session;
    g_self = this;
    AoaControlTap::instance().begin();
    AoaControlTap::instance().setHypothesisRespond(true);

    // Desk HYPOTHESIS: already-in-accessory-mode Google VID/PID. Not NBT FACT.
    USB.VID(vid_);
    USB.PID(pid_);
    USB.manufacturerName("Google");
    USB.productName("AOA Observe");
    USB.serialNumber("HUPROBE-AOA0");
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
    DiagLog::printf("[AOA] aoa_observe ready vid=0x%04X pid=0x%04X (%s) soft=%d\n",
                    (unsigned)vid_, (unsigned)pid_, identityNote(), soft_attached_ ? 1 : 0);
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
void AoaObserveGadget::begin(Session*, bool) {}
void AoaObserveGadget::loop() {}
void AoaObserveGadget::notifyUsbStarted() {}
void AoaObserveGadget::notifyUsbStopped() {}
bool AoaObserveGadget::softDetach(uint16_t) { return false; }
bool AoaObserveGadget::softAttach() { return false; }
bool AoaObserveGadget::softCycle(uint16_t) { return false; }
size_t AoaObserveGadget::drainVendorRx(uint8_t*, size_t) { return 0; }
bool AoaObserveGadget::vendorMounted() const { return false; }

}  // namespace aoa
}  // namespace huprobe

#endif
