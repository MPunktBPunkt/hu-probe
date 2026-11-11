/**
 * @file UsbCoreTap.cpp
 * @brief UsbCoreTap implementation plus the `--wrap=dcd_event_handler` linker hook.
 */
#include "core/UsbCoreTap.h"
#include "core/ProbeClock.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

#if !ARDUINO_USB_MODE
#include "tusb.h"
#include "device/dcd.h"
#endif

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

uint32_t UsbCoreTap::events_ = 0;
uint32_t UsbCoreTap::busReset_ = 0;
uint32_t UsbCoreTap::unplugged_ = 0;
uint32_t UsbCoreTap::suspend_ = 0;
uint32_t UsbCoreTap::resume_ = 0;
uint32_t UsbCoreTap::sof_ = 0;
uint32_t UsbCoreTap::setup_ = 0;
uint32_t UsbCoreTap::setupStd_[kStdRequests] = {0};
uint32_t UsbCoreTap::setupStdOther_ = 0;
uint32_t UsbCoreTap::setupClass_ = 0;
uint32_t UsbCoreTap::setupVendor_ = 0;
uint32_t UsbCoreTap::setupReserved_ = 0;
uint32_t UsbCoreTap::descDevice_ = 0;
uint32_t UsbCoreTap::descConfig_ = 0;
uint32_t UsbCoreTap::descString_ = 0;
uint32_t UsbCoreTap::descMsOs_ = 0;
uint32_t UsbCoreTap::descQualifier_ = 0;
uint32_t UsbCoreTap::descOtherSpeed_ = 0;
uint32_t UsbCoreTap::descBos_ = 0;
uint32_t UsbCoreTap::descOther_ = 0;
uint32_t UsbCoreTap::xferOk_ = 0;
uint32_t UsbCoreTap::xferFailed_ = 0;
uint32_t UsbCoreTap::xferStalled_ = 0;
uint32_t UsbCoreTap::xferEp0_ = 0;
uint32_t UsbCoreTap::ringDrops_ = 0;
int64_t UsbCoreTap::lastBusResetUs_ = 0;
int64_t UsbCoreTap::lastSetupUs_ = 0;
uint8_t UsbCoreTap::lastSetup_[8] = {0};
bool UsbCoreTap::ringEnabled_ = false;
UsbCoreTap::Sample UsbCoreTap::ring_[kRingDepth];
size_t UsbCoreTap::head_ = 0;
size_t UsbCoreTap::count_ = 0;

void UsbCoreTap::setRingEnabled(bool on) {
    portENTER_CRITICAL(&s_mux);
    ringEnabled_ = on;
    head_ = 0;
    count_ = 0;
    portEXIT_CRITICAL(&s_mux);
}

bool UsbCoreTap::pop(Sample& out) {
    bool ok = false;
    portENTER_CRITICAL(&s_mux);
    if (count_ > 0) {
        out = ring_[head_];
        head_ = (head_ + 1) % kRingDepth;
        count_--;
        ok = true;
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

void IRAM_ATTR UsbCoreTap::onDcdEvent(const void* event) {
#if !ARDUINO_USB_MODE
    const dcd_event_t* e = static_cast<const dcd_event_t*>(event);
    if (!e) return;
    Sample s;
    s.tUs = ProbeClock::nowUs();
    bool queue = true;
    portENTER_CRITICAL_SAFE(&s_mux);
    events_++;
    switch (e->event_id) {
        case DCD_EVENT_SOF:
            sof_++;
            queue = false;
            break;
        case DCD_EVENT_BUS_RESET:
            busReset_++;
            lastBusResetUs_ = s.tUs;
            s.ev = Ev::BusReset;
            s.speed = (uint8_t)e->bus_reset.speed;
            break;
        case DCD_EVENT_UNPLUGGED:
            unplugged_++;
            s.ev = Ev::Unplugged;
            break;
        case DCD_EVENT_SUSPEND:
            suspend_++;
            s.ev = Ev::Suspend;
            break;
        case DCD_EVENT_RESUME:
            resume_++;
            s.ev = Ev::Resume;
            break;
        case DCD_EVENT_SETUP_RECEIVED: {
            s.ev = Ev::Setup;
            memcpy(s.setup, &e->setup_received, 8);
            memcpy(lastSetup_, s.setup, 8);
            lastSetupUs_ = s.tUs;
            setup_++;
            const uint8_t bm = s.setup[0];
            const uint8_t req = s.setup[1];
            switch ((bm >> 5) & 0x3) {
                case 0:
                    if (req < kStdRequests) setupStd_[req]++;
                    else setupStdOther_++;
                    if (req == 6) {  // GET_DESCRIPTOR: wValue = type << 8 | index
                        const uint8_t type = s.setup[3];
                        const uint8_t index = s.setup[2];
                        if (type == 1) descDevice_++;
                        else if (type == 2) descConfig_++;
                        else if (type == 3 && index == 0xEE) descMsOs_++;
                        else if (type == 3) descString_++;
                        else if (type == 6) descQualifier_++;
                        else if (type == 7) descOtherSpeed_++;
                        else if (type == 0x0F) descBos_++;
                        else descOther_++;
                    }
                    break;
                case 1: setupClass_++; break;
                case 2: setupVendor_++; break;
                default: setupReserved_++; break;
            }
            break;
        }
        case DCD_EVENT_XFER_COMPLETE:
            s.ev = Ev::XferComplete;
            s.epAddr = e->xfer_complete.ep_addr;
            s.result = e->xfer_complete.result;
            s.len = e->xfer_complete.len;
            if (s.result == XFER_RESULT_SUCCESS) xferOk_++;
            else if (s.result == XFER_RESULT_STALLED) xferStalled_++;
            else xferFailed_++;
            if ((s.epAddr & 0x7F) == 0) xferEp0_++;
            break;
        default:
            queue = false;
            break;
    }
    if (queue && ringEnabled_) {
        if (count_ < kRingDepth) {
            ring_[(head_ + count_) % kRingDepth] = s;
            count_++;
        } else {
            ringDrops_++;
        }
    }
    portEXIT_CRITICAL_SAFE(&s_mux);
#else
    (void)event;
#endif
}

const char* UsbCoreTap::standardRequestName(uint8_t bRequest) {
    switch (bRequest) {
        case 0: return "GET_STATUS";
        case 1: return "CLEAR_FEATURE";
        case 3: return "SET_FEATURE";
        case 5: return "SET_ADDRESS";
        case 6: return "GET_DESCRIPTOR";
        case 7: return "SET_DESCRIPTOR";
        case 8: return "GET_CONFIGURATION";
        case 9: return "SET_CONFIGURATION";
        case 10: return "GET_INTERFACE";
        case 11: return "SET_INTERFACE";
        case 12: return "SYNCH_FRAME";
        default: return nullptr;
    }
}

const char* UsbCoreTap::descriptorTypeName(uint8_t type) {
    switch (type) {
        case 1: return "device";
        case 2: return "configuration";
        case 3: return "string";
        case 4: return "interface";
        case 5: return "endpoint";
        case 6: return "device_qualifier";
        case 7: return "other_speed_configuration";
        case 0x0F: return "bos";
        case 0x21: return "hid";
        case 0x22: return "hid_report";
        default: return nullptr;
    }
}

void UsbCoreTap::toJson(JsonObject obj) {
    obj["hook"] = "dcd_event_handler";
    obj["hook_live"] = hookLive();
    obj["scope"] = "since_boot";
    obj["events"] = events_;
    obj["bus_reset"] = busReset_;
    obj["unplugged"] = unplugged_;
    obj["suspend"] = suspend_;
    obj["resume"] = resume_;
    obj["sof"] = sof_;
    if (lastBusResetUs_) obj["last_bus_reset_t_us"] = (long long)lastBusResetUs_;

    JsonObject setup = obj["setup"].to<JsonObject>();
    setup["total"] = setup_;
    setup["class"] = setupClass_;
    setup["vendor"] = setupVendor_;
    setup["reserved"] = setupReserved_;
    JsonObject std = setup["standard"].to<JsonObject>();
    for (uint8_t i = 0; i < kStdRequests; i++) {
        if (!setupStd_[i]) continue;
        const char* name = standardRequestName(i);
        if (name) {
            std[name] = setupStd_[i];
        } else {
            char key[8];
            snprintf(key, sizeof(key), "req_%u", (unsigned)i);
            std[key] = setupStd_[i];
        }
    }
    if (setupStdOther_) std["other"] = setupStdOther_;

    JsonObject desc = obj["get_descriptor"].to<JsonObject>();
    desc["device"] = descDevice_;
    desc["configuration"] = descConfig_;
    desc["string"] = descString_;
    desc["ms_os_string_ee"] = descMsOs_;
    desc["device_qualifier"] = descQualifier_;
    desc["other_speed_configuration"] = descOtherSpeed_;
    desc["bos"] = descBos_;
    desc["other"] = descOther_;

    JsonObject xfer = obj["xfer"].to<JsonObject>();
    xfer["ok"] = xferOk_;
    xfer["stalled"] = xferStalled_;
    xfer["failed"] = xferFailed_;
    xfer["ep0"] = xferEp0_;

    obj["ring_enabled"] = (bool)ringEnabled_;
    obj["ring_drops"] = ringDrops_;
    if (lastSetupUs_) {
        char hex[17];
        for (int i = 0; i < 8; i++) snprintf(hex + 2 * i, 3, "%02x", lastSetup_[i]);
        obj["last_setup_hex"] = hex;
        obj["last_setup_t_us"] = (long long)lastSetupUs_;
    }
}

#if !ARDUINO_USB_MODE
extern "C" {

void __real_dcd_event_handler(dcd_event_t const* event, bool in_isr);

/**
 * @brief Linker wrap of TinyUSB dcd_event_handler: record the event, then hand it to usbd.
 * @param event   DCD event from the controller driver.
 * @param in_isr  True when called from the USB interrupt.
 * @note USB interrupt or task context.
 */
void IRAM_ATTR __wrap_dcd_event_handler(dcd_event_t const* event, bool in_isr) {
    UsbCoreTap::onDcdEvent(event);
    __real_dcd_event_handler(event, in_isr);
}

}  // extern "C"
#endif
