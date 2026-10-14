#include "MscBotTap.h"
#include "tusb.h"
#include "class/msc/msc.h"
#include <cstring>

#if !ARDUINO_USB_MODE

// TinyUSB 0.15 `_mscd_itf` (DWARF): cbw@0 (31), csw@32 (13), stage@48, …
extern "C" uint8_t _mscd_itf[];

enum {
    kOffCbw = 0,
    kOffCsw = 32,
    kOffStage = 48,
    kStageData = 1,
    kStageStatus = 2,
    kStageStatusSent = 3,
    kStageNeedReset = 4
};

static const uint8_t* itfCbw() { return _mscd_itf + kOffCbw; }
static const uint8_t* itfCsw() { return _mscd_itf + kOffCsw; }
static uint8_t itfStage() { return _mscd_itf[kOffStage]; }

static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

ReadInstrument* MscBotTap::instrument_ = nullptr;
uint32_t MscBotTap::lastCbwTag_ = 0xFFFFFFFFu;
bool MscBotTap::cswPending_ = false;
bool MscBotTap::needResetArmed_ = false;
MscBotTap::Pending MscBotTap::pending_[kPendingRing];
volatile size_t MscBotTap::pendingHead_ = 0;
volatile size_t MscBotTap::pendingTail_ = 0;
volatile size_t MscBotTap::pendingCount_ = 0;
uint32_t MscBotTap::pendingOverflow_ = 0;

void MscBotTap::begin(ReadInstrument* instrument) {
    instrument_ = instrument;
    lastCbwTag_ = 0xFFFFFFFFu;
    cswPending_ = false;
    needResetArmed_ = false;
    pendingHead_ = pendingTail_ = pendingCount_ = 0;
    pendingOverflow_ = 0;
}

ReadInstrument* MscBotTap::instrument() { return instrument_; }

void MscBotTap::queue(const Pending& p) {
    if (pendingCount_ >= kPendingRing) {
        pendingHead_ = (pendingHead_ + 1) % kPendingRing;
        pendingCount_--;
        pendingOverflow_++;
    }
    pending_[pendingTail_] = p;
    pendingTail_ = (pendingTail_ + 1) % kPendingRing;
    pendingCount_++;
}

void MscBotTap::queueControl(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue,
                             uint16_t wIndex, uint16_t wLength) {
    Pending p;
    p.kind = Kind::Control;
    p.bmRequestType = bmRequestType;
    p.bRequest = bRequest;
    p.wValue = wValue;
    p.wIndex = wIndex;
    p.wLength = wLength;
    queue(p);
}

void MscBotTap::emitCbwIfNew() {
    const uint8_t* cbw = itfCbw();
    if (readLe32(cbw) != MSC_CBW_SIGNATURE) return;
    uint32_t tag = readLe32(cbw + 4);
    if (tag == lastCbwTag_ && cswPending_) return;
    lastCbwTag_ = tag;
    cswPending_ = true;
    needResetArmed_ = false;
    Pending p;
    p.kind = Kind::Cbw;
    p.botTag = tag;
    memcpy(p.raw, cbw, 31);
    queue(p);
}

void MscBotTap::emitDataChunk(uint32_t lba, uint32_t nBytes, uint32_t durUs, uint32_t gapUs,
                              const char* tag, const char* kind, const char* dir) {
    Pending p;
    p.kind = Kind::Data;
    p.botTag = currentHostTag();
    p.lba = lba;
    p.nBytes = nBytes;
    p.durUs = durUs;
    p.gapUs = gapUs;
    if (tag) snprintf(p.tag, sizeof(p.tag), "%s", tag);
    if (kind) snprintf(p.kindStr, sizeof(p.kindStr), "%s", kind);
    if (dir) snprintf(p.dir, sizeof(p.dir), "%s", dir);
    queue(p);
}

void MscBotTap::emitCsw() {
    if (!cswPending_) return;
    const uint8_t* csw = itfCsw();
    if (readLe32(csw) != MSC_CSW_SIGNATURE) return;
    Pending p;
    p.kind = Kind::Csw;
    p.botTag = readLe32(csw + 4);
    memcpy(p.raw, csw, 13);
    queue(p);
    cswPending_ = false;
}

void MscBotTap::emitNeedResetIfNew() {
    if (needResetArmed_) return;
    needResetArmed_ = true;
    cswPending_ = false;
    Pending p;
    p.kind = Kind::NeedReset;
    queue(p);
}

void MscBotTap::onBotReset() { needResetArmed_ = false; }

void MscBotTap::onNonDataScsiEnter() { emitCbwIfNew(); }

void MscBotTap::afterMscdXfer() {
    uint8_t st = itfStage();
    if (st == kStageNeedReset) {
        emitNeedResetIfNew();
        return;
    }
    if (st == kStageData) {
        emitCbwIfNew();
    } else if (st == kStageStatus) {
        emitCbwIfNew();
        emitCsw();
    } else if (st == kStageStatusSent) {
        emitCsw();
    }
}

void MscBotTap::drain(int maxEvents) {
    if (!instrument_) {
        pendingHead_ = pendingTail_ = pendingCount_ = 0;
        return;
    }
    for (int i = 0; i < maxEvents && pendingCount_ > 0; i++) {
        Pending p = pending_[pendingHead_];
        pendingHead_ = (pendingHead_ + 1) % kPendingRing;
        pendingCount_--;
        switch (p.kind) {
            case Kind::Cbw:
                instrument_->emitBotCbwTinyusb(p.raw);
                break;
            case Kind::Data:
                instrument_->emitBotDataTinyusb(p.botTag, p.lba, p.nBytes, p.durUs, p.gapUs, p.tag,
                                                p.kindStr, p.dir);
                break;
            case Kind::Csw:
                instrument_->emitBotCswTinyusb(p.raw);
                break;
            case Kind::NeedReset:
                instrument_->emitBotNeedResetTinyusb();
                break;
            case Kind::Control:
                instrument_->emitUsbControlTinyusb(p.bmRequestType, p.bRequest, p.wValue, p.wIndex,
                                                   p.wLength);
                break;
        }
    }
}

void MscBotTap::poll() {
    if (itfStage() == kStageNeedReset) {
        emitNeedResetIfNew();
    } else if (cswPending_) {
        uint8_t st = itfStage();
        if (st == kStageStatus || st == kStageStatusSent) {
            emitCsw();
        }
    }
    drain(96);
}

uint32_t MscBotTap::currentHostTag() {
    const uint8_t* cbw = itfCbw();
    if (readLe32(cbw) == MSC_CBW_SIGNATURE) return readLe32(cbw + 4);
    return lastCbwTag_;
}

extern "C" {

bool __real_tud_msc_test_unit_ready_cb(uint8_t lun);
void __real_tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
                               uint8_t product_rev[4]);
void __real_tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size);
int32_t __real_tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                               uint16_t bufsize);
bool __real_tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                                  bool load_eject);
bool __real_mscd_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                 tusb_control_request_t const* request);
bool __real_mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                         uint32_t xferred_bytes);

bool __wrap_mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                         uint32_t xferred_bytes) {
    bool ok = __real_mscd_xfer_cb(rhport, ep_addr, result, xferred_bytes);
    MscBotTap::afterMscdXfer();
    return ok;
}

bool __wrap_tud_msc_test_unit_ready_cb(uint8_t lun) {
    MscBotTap::onNonDataScsiEnter();
    return __real_tud_msc_test_unit_ready_cb(lun);
}

void __wrap_tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
                               uint8_t product_rev[4]) {
    MscBotTap::onNonDataScsiEnter();
    __real_tud_msc_inquiry_cb(lun, vendor_id, product_id, product_rev);
}

void __wrap_tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    MscBotTap::onNonDataScsiEnter();
    __real_tud_msc_capacity_cb(lun, block_count, block_size);
}

int32_t __wrap_tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                               uint16_t bufsize) {
    MscBotTap::onNonDataScsiEnter();
    return __real_tud_msc_scsi_cb(lun, scsi_cmd, buffer, bufsize);
}

bool __wrap_tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                                  bool load_eject) {
    MscBotTap::onNonDataScsiEnter();
    return __real_tud_msc_start_stop_cb(lun, power_condition, start, load_eject);
}

bool __wrap_mscd_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                 tusb_control_request_t const* request) {
    if (request && stage == CONTROL_STAGE_SETUP) {
        MscBotTap::queueControl(request->bmRequestType, request->bRequest, request->wValue,
                                request->wIndex, request->wLength);
    }
    bool ok = __real_mscd_control_xfer_cb(rhport, stage, request);
    if (request && stage == CONTROL_STAGE_SETUP && (request->bmRequestType & 0x60) == 0x20 &&
        request->bRequest == 0xFF) {
        MscBotTap::onBotReset();
    }
    return ok;
}

void tud_msc_read10_complete_cb(uint8_t lun) {
    (void)lun;
    MscBotTap::emitCsw();
}

void tud_msc_write10_complete_cb(uint8_t lun) {
    (void)lun;
    MscBotTap::emitCsw();
}

void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]) {
    (void)lun;
    (void)scsi_cmd;
    MscBotTap::emitCsw();
}

}  // extern "C"

#else

void MscBotTap::begin(ReadInstrument*) {}
ReadInstrument* MscBotTap::instrument() { return nullptr; }
void MscBotTap::emitCbwIfNew() {}
void MscBotTap::emitDataChunk(uint32_t, uint32_t, uint32_t, uint32_t, const char*, const char*,
                              const char*) {}
void MscBotTap::emitCsw() {}
void MscBotTap::emitNeedResetIfNew() {}
void MscBotTap::onBotReset() {}
void MscBotTap::onNonDataScsiEnter() {}
void MscBotTap::afterMscdXfer() {}
void MscBotTap::poll() {}
void MscBotTap::drain(int) {}
void MscBotTap::queue(const Pending&) {}
void MscBotTap::queueControl(uint8_t, uint8_t, uint16_t, uint16_t, uint16_t) {}
uint32_t MscBotTap::currentHostTag() { return 0; }

ReadInstrument* MscBotTap::instrument_ = nullptr;
uint32_t MscBotTap::lastCbwTag_ = 0;
bool MscBotTap::cswPending_ = false;
bool MscBotTap::needResetArmed_ = false;
MscBotTap::Pending MscBotTap::pending_[kPendingRing];
volatile size_t MscBotTap::pendingHead_ = 0;
volatile size_t MscBotTap::pendingTail_ = 0;
volatile size_t MscBotTap::pendingCount_ = 0;
uint32_t MscBotTap::pendingOverflow_ = 0;

#endif
