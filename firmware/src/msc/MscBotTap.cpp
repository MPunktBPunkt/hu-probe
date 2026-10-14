#include "MscBotTap.h"
#include "tusb.h"
#include "class/msc/msc.h"
#include <cstring>

#if !ARDUINO_USB_MODE

// TinyUSB 0.15 `_mscd_itf` (DWARF): cbw@0 (31), csw@32 (13), stage@48, …
// Opaque so C++ packing cannot diverge from the C object in libarduino_tinyusb.
extern "C" uint8_t _mscd_itf[];

enum {
    kOffCbw = 0,
    kOffCsw = 32,
    kOffStage = 48,
    kStageCmd = 0,
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

void MscBotTap::begin(ReadInstrument* instrument) {
    instrument_ = instrument;
    lastCbwTag_ = 0xFFFFFFFFu;
    cswPending_ = false;
    needResetArmed_ = false;
}

ReadInstrument* MscBotTap::instrument() { return instrument_; }

void MscBotTap::emitCbwIfNew() {
    if (!instrument_) return;
    const uint8_t* cbw = itfCbw();
    if (readLe32(cbw) != MSC_CBW_SIGNATURE) return;
    uint32_t tag = readLe32(cbw + 4);
    if (tag == lastCbwTag_ && cswPending_) return;
    lastCbwTag_ = tag;
    cswPending_ = true;
    needResetArmed_ = false;
    instrument_->emitBotCbwTinyusb(cbw);
}

void MscBotTap::emitDataChunk(uint32_t lba, uint32_t nBytes, uint32_t durUs, uint32_t gapUs,
                              const char* tag, const char* kind, const char* dir) {
    if (!instrument_) return;
    instrument_->emitBotDataTinyusb(currentHostTag(), lba, nBytes, durUs, gapUs, tag, kind, dir);
}

void MscBotTap::emitCsw() {
    if (!instrument_ || !cswPending_) return;
    const uint8_t* csw = itfCsw();
    if (readLe32(csw) != MSC_CSW_SIGNATURE) return;
    instrument_->emitBotCswTinyusb(csw);
    cswPending_ = false;
}

void MscBotTap::emitNeedResetIfNew() {
    if (!instrument_ || needResetArmed_) return;
    needResetArmed_ = true;
    cswPending_ = false;
    instrument_->emitBotNeedResetTinyusb();
}

void MscBotTap::onBotReset() { needResetArmed_ = false; }

void MscBotTap::onNonDataScsiEnter() { emitCbwIfNew(); }

void MscBotTap::afterMscdXfer() {
    uint8_t st = itfStage();
    if (st == kStageNeedReset) {
        emitNeedResetIfNew();
        return;
    }
    // CBW is in _mscd_itf once TinyUSB leaves CMD for DATA (bulk) or STATUS (no-data).
    // Do not emit on CMD/STATUS_SENT — residual CBW bytes would duplicate after CSW.
    if (st == kStageData) {
        emitCbwIfNew();
    } else if (st == kStageStatus) {
        emitCbwIfNew();
        emitCsw();
    } else if (st == kStageStatusSent) {
        emitCsw();
    }
}

void MscBotTap::poll() {
    // TUR/INQUIRY/CAPACITY build CSW after the Arduino callback returns — catch STATUS stage.
    if (itfStage() == kStageNeedReset) {
        emitNeedResetIfNew();
        return;
    }
    if (!cswPending_) return;
    uint8_t st = itfStage();
    if (st == kStageStatus || st == kStageStatusSent) {
        emitCsw();
    }
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
    if (request && stage == CONTROL_STAGE_SETUP && MscBotTap::instrument()) {
        MscBotTap::instrument()->emitUsbControlTinyusb(
            request->bmRequestType, request->bRequest, request->wValue, request->wIndex,
            request->wLength);
    }
    bool ok = __real_mscd_control_xfer_cb(rhport, stage, request);
    // BOT_RESET clears NEED_RESET; allow a later need_reset event.
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
uint32_t MscBotTap::currentHostTag() { return 0; }

ReadInstrument* MscBotTap::instrument_ = nullptr;
uint32_t MscBotTap::lastCbwTag_ = 0;
bool MscBotTap::cswPending_ = false;
bool MscBotTap::needResetArmed_ = false;

#endif
