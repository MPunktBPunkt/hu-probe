/**
 * @file MscBotTap.cpp
 * @brief MscBotTap implementation plus the `--wrap` linker hooks around TinyUSB MSC/USBD functions.
 */
#include "MscBotTap.h"
#include "BuildFlags.h"
#include "capture/AutoMarker.h"
#include "capture/OfflineCapture.h"
#include "core/DiagLog.h"
#include "core/PanicTail.h"
#include "core/ProbeClock.h"
#include "tusb.h"
#include "class/msc/msc.h"
#include <cstring>

#if !ARDUINO_USB_MODE

// TinyUSB 0.15 `_mscd_itf` (DWARF): cbw@0 (31), csw@32 (13), stage@48, …
extern "C" uint8_t _mscd_itf[];

/** @brief Byte offsets into `_mscd_itf` and TinyUSB 0.15 MSC stage values. */
enum {
    kOffCbw = 0,
    kOffCsw = 32,
    kOffStage = 48,
    kStageData = 1,
    kStageStatus = 2,
    kStageStatusSent = 3,
    kStageNeedReset = 4
};

/** @brief Pointer to the 31-byte CBW inside TinyUSB's `_mscd_itf`. */
static const uint8_t* itfCbw() { return _mscd_itf + kOffCbw; }

/** @brief Pointer to the 13-byte CSW inside TinyUSB's `_mscd_itf`. */
static const uint8_t* itfCsw() { return _mscd_itf + kOffCsw; }

/** @brief Current TinyUSB MSC stage (kStageData, kStageStatus, kStageStatusSent, kStageNeedReset). */
static uint8_t itfStage() { return _mscd_itf[kOffStage]; }

/**
 * @brief Read a little-endian 32-bit value.
 * @param p  Source; must have 4 readable bytes.
 * @return Decoded value.
 */
static uint32_t readLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

ReadInstrument* MscBotTap::instrument_ = nullptr;
uint32_t MscBotTap::lastCbwTag_ = 0xFFFFFFFFu;
bool MscBotTap::cswPending_ = false;
bool MscBotTap::needResetArmed_ = false;
int64_t MscBotTap::lastMountTUs_ = 0;
int64_t MscBotTap::lastUmountTUs_ = 0;
char MscBotTap::probeSerial_[16] = "HP0001";
MscBotTap::Pending MscBotTap::pending_[kPendingRing];
volatile size_t MscBotTap::pendingHead_ = 0;
volatile size_t MscBotTap::pendingTail_ = 0;
volatile size_t MscBotTap::pendingCount_ = 0;
uint32_t MscBotTap::pendingOverflow_ = 0;

void MscBotTap::begin(ReadInstrument* instrument) {
    instrument_ = instrument;
    clearBotEpoch();
    pendingHead_ = pendingTail_ = pendingCount_ = 0;
    pendingOverflow_ = 0;
}

ReadInstrument* MscBotTap::instrument() { return instrument_; }

void MscBotTap::setProbeSerial(const char* serial) {
    if (!serial) return;
    snprintf(probeSerial_, sizeof(probeSerial_), "%s", serial);
}

void MscBotTap::clearBotEpoch() {
    lastCbwTag_ = 0xFFFFFFFFu;
    cswPending_ = false;
    needResetArmed_ = false;
}

void MscBotTap::clearNeedReset() {
    if (!needResetArmed_) return;
    needResetArmed_ = false;
    DiagLog::printf("[MSC] need_reset cleared (intentional detach)\n");
}

void MscBotTap::queue(const Pending& p) {
    // Raise watermark at queue (USB callback) so status/health cannot stamp below
    // pending Capture-tUs before drain publishes them.
    ProbeClock::observeEventTUs(p.tUs);
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
    p.tUs = ProbeClock::nowUs();
    p.bmRequestType = bmRequestType;
    p.bRequest = bRequest;
    p.wValue = wValue;
    p.wIndex = wIndex;
    p.wLength = wLength;
    // Standard CLEAR_FEATURE(HALT) while NEED_RESET: log, do not heal.
    if ((bmRequestType & 0x60) == 0 && bRequest == 0x01 && needResetArmed_) {
        p.flag = 1;
    }
    queue(p);
}

void MscBotTap::emitCbwIfNew(int64_t tUs) {
    const uint8_t* cbw = itfCbw();
    if (readLe32(cbw) != MSC_CBW_SIGNATURE) return;
    uint32_t tag = readLe32(cbw + 4);
    if (tag == lastCbwTag_ && cswPending_) return;
    lastCbwTag_ = tag;
    cswPending_ = true;
    needResetArmed_ = false;
    Pending p;
    p.kind = Kind::Cbw;
    p.tUs = tUs;
    p.botTag = tag;
    memcpy(p.raw, cbw, 31);
    queue(p);
}

void MscBotTap::emitDataChunk(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs,
                              uint32_t gapUs, const char* tag, const char* kind, const char* dir) {
    Pending p;
    p.kind = Kind::Data;
    p.tUs = tUs;
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
    p.tUs = ProbeClock::nowUs();
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
    p.tUs = ProbeClock::nowUs();
    PanicTail::breadcrumb(PanicTail::NeedReset);
    queue(p);
}

void MscBotTap::onBotReset() {
    Pending p;
    p.kind = Kind::BotReset;
    p.tUs = ProbeClock::nowUs();
    PanicTail::breadcrumb(PanicTail::BotReset);
    queue(p);
    clearBotEpoch();
}

void MscBotTap::queueSetAddress(uint8_t addr) {
    Pending p;
    p.kind = Kind::SetAddress;
    p.tUs = ProbeClock::nowUs();
    p.wValue = addr;
    queue(p);
}

void MscBotTap::queueMount() {
    // Replug without VBUS often skips umount — Mount is the safe epoch (Audit 007/008).
    clearBotEpoch();
    Pending p;
    p.kind = Kind::Mount;
    p.tUs = ProbeClock::nowUs();
    lastMountTUs_ = p.tUs;
    PanicTail::breadcrumb(PanicTail::UsbMount);
    p.wValue = 1;  // SET_CONFIGURATION n≠0 implied; exact n not exposed by tud_mount_cb
    queue(p);
}

void MscBotTap::queueUmount() {
    Pending p;
    p.kind = Kind::Umount;
    p.tUs = ProbeClock::nowUs();
    lastUmountTUs_ = p.tUs;
    PanicTail::breadcrumb(PanicTail::UsbUmount);
    p.wValue = 0;  // cfg 0 or bus-level unmount
    queue(p);
    clearBotEpoch();
}

void MscBotTap::queueEpStall(uint8_t epAddr) {
    Pending p;
    p.kind = Kind::EpStall;
    p.tUs = ProbeClock::nowUs();
    p.epAddr = epAddr;
    PanicTail::breadcrumb(PanicTail::EpStall, epAddr);
    queue(p);
}

void MscBotTap::queueEpClearHalt(uint8_t epAddr) {
    Pending p;
    p.kind = Kind::EpClearHalt;
    p.tUs = ProbeClock::nowUs();
    p.epAddr = epAddr;
    // Capture latch at queue time — Clear-Halt must NOT clear NEED_RESET.
    p.flag = needResetArmed_ ? 1 : 0;
    queue(p);
}

void MscBotTap::onNonDataScsiEnter() { emitCbwIfNew(ProbeClock::nowUs()); }

void MscBotTap::afterMscdXfer() {
    uint8_t st = itfStage();
    if (st == kStageNeedReset) {
        emitNeedResetIfNew();
        return;
    }
    int64_t now = ProbeClock::nowUs();
    if (st == kStageData) {
        emitCbwIfNew(now);
    } else if (st == kStageStatus) {
        emitCbwIfNew(now);
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
    const bool offline = OfflineCapture::instance().acceptsPending();
    for (int i = 0; i < maxEvents && pendingCount_ > 0; i++) {
        Pending p = pending_[pendingHead_];
        pendingHead_ = (pendingHead_ + 1) % kPendingRing;
        pendingCount_--;
        if (offline) {
            OfflineCapture::instance().appendPending(
                static_cast<uint8_t>(p.kind), p.tUs, p.raw, p.botTag, p.lba, p.nBytes, p.durUs,
                p.gapUs, p.wValue, p.wIndex, p.wLength, p.bmRequestType, p.bRequest, p.epAddr,
                p.flag, p.tag, p.kindStr, p.dir);
            AutoMarker::observe(static_cast<uint8_t>(p.kind), p.tUs, p.raw);
            continue;
        }
        AutoMarker::observe(static_cast<uint8_t>(p.kind), p.tUs, p.raw);
        switch (p.kind) {
            case Kind::Cbw:
                instrument_->emitBotCbwTinyusb(p.raw, p.tUs);
                break;
            case Kind::Data:
                instrument_->emitBotDataTinyusb(p.tUs, p.botTag, p.lba, p.nBytes, p.durUs, p.gapUs,
                                                p.tag, p.kindStr, p.dir);
                break;
            case Kind::Csw:
                instrument_->emitBotCswTinyusb(p.raw, p.tUs);
                break;
            case Kind::NeedReset:
                instrument_->emitBotNeedResetTinyusb(p.tUs);
                break;
            case Kind::Control:
                instrument_->emitUsbControlTinyusb(p.bmRequestType, p.bRequest, p.wValue, p.wIndex,
                                                   p.wLength, p.tUs, p.flag != 0);
                break;
            case Kind::SetAddress:
                instrument_->emitUsbEpochTinyusb(p.tUs, "set_address", 0, p.wValue, nullptr);
                break;
            case Kind::Mount:
                // Mount ≡ SET_CONFIGURATION n; identity+phase share Capture-tUs (no race).
                instrument_->emitUsbEpochTinyusb(p.tUs, "mount", 0, p.wValue,
                                                 "set_configuration_n_via_tud_mount_cb");
                instrument_->emitIdentityDescriptorsAt(p.tUs, probeSerial_, "HUPROBE", "HU_PROBE",
                                                       FW_VERSION);
                instrument_->emitPhaseAt(p.tUs, "enum", "plug");
                break;
            case Kind::Umount:
                instrument_->emitUsbEpochTinyusb(p.tUs, "umount", 0, 0,
                                                 "set_configuration_0_or_bus_via_tud_umount_cb");
                instrument_->emitPhaseAt(p.tUs, "idle", "unplug");
                break;
            case Kind::EpStall:
                instrument_->emitUsbEpochTinyusb(p.tUs, "ep_stall", p.epAddr, 0, nullptr);
                break;
            case Kind::EpClearHalt:
                instrument_->emitUsbEpochTinyusb(
                    p.tUs, "ep_clear_halt", p.epAddr, 0,
                    p.flag ? "need_reset_active_not_healed" : nullptr);
                break;
            case Kind::BotReset:
                instrument_->emitUsbEpochTinyusb(p.tUs, "msc_bot_reset", 0, 0, "msc_req_reset_0xff");
                break;
        }
        // Leave time for WiFi/lwIP while serializing BOT bursts.
        if ((i & 7) == 7) yield();
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
    // Small batches + yield in drain: OTG host presence kills WiFi assoc otherwise.
    drain(16);
}

uint32_t MscBotTap::currentHostTag() {
    const uint8_t* cbw = itfCbw();
    if (readLe32(cbw) == MSC_CBW_SIGNATURE) return readLe32(cbw + 4);
    return lastCbwTag_;
}

uint32_t MscBotTap::currentHostXferLen() {
    const uint8_t* cbw = itfCbw();
    return readLe32(cbw) == MSC_CBW_SIGNATURE ? readLe32(cbw + 8) : 0;
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

/**
 * @brief Linker wrap of TinyUSB mscd_xfer_cb: run the real handler, then tap the new MSC stage.
 * @param rhport         Root hub port.
 * @param ep_addr        Endpoint address of the completed transfer.
 * @param result         Transfer result.
 * @param xferred_bytes  Number of bytes transferred.
 * @return Result of the real mscd_xfer_cb.
 * @note USB task context.
 */
bool __wrap_mscd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                         uint32_t xferred_bytes) {
    bool ok = __real_mscd_xfer_cb(rhport, ep_addr, result, xferred_bytes);
    MscBotTap::afterMscdXfer();
    return ok;
}

/**
 * @brief Linker wrap of tud_msc_test_unit_ready_cb: tap the CBW and inject a pending UNIT ATTENTION.
 *
 * If the real callback reports ready and a UNIT ATTENTION is pending, sets sense 06/28/00
 * and fails the TUR once.
 *
 * @param lun  Logical unit number.
 * @return false if not ready or a UNIT ATTENTION was injected, true otherwise.
 * @note USB task context.
 */
bool __wrap_tud_msc_test_unit_ready_cb(uint8_t lun) {
    MscBotTap::onNonDataScsiEnter();
    const bool ready = __real_tud_msc_test_unit_ready_cb(lun);
    // Hidden medium already reports NOT READY 3A; UA stays pending until it is present.
    if (ready && MscBotTap::consumeUnitAttention()) {
        tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00);
        return false;
    }
    return ready;
}

/**
 * @brief Linker wrap of tud_msc_inquiry_cb: tap the CBW, then fill INQUIRY via the real callback.
 * @param[in]  lun          Logical unit number.
 * @param[out] vendor_id    Vendor ID (8 bytes).
 * @param[out] product_id   Product ID (16 bytes).
 * @param[out] product_rev  Product revision (4 bytes).
 * @note USB task context.
 */
void __wrap_tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16],
                               uint8_t product_rev[4]) {
    MscBotTap::onNonDataScsiEnter();
    __real_tud_msc_inquiry_cb(lun, vendor_id, product_id, product_rev);
}

/**
 * @brief Linker wrap of tud_msc_capacity_cb: tap the CBW, then report capacity via the real callback.
 * @param[in]  lun          Logical unit number.
 * @param[out] block_count  Number of blocks.
 * @param[out] block_size   Block size in bytes.
 * @note USB task context.
 */
void __wrap_tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    MscBotTap::onNonDataScsiEnter();
    __real_tud_msc_capacity_cb(lun, block_count, block_size);
}

/**
 * @brief Linker wrap of tud_msc_scsi_cb: tap the CBW, then handle the command via the real callback.
 * @param lun       Logical unit number.
 * @param scsi_cmd  16-byte SCSI CDB.
 * @param buffer    Data buffer for the response.
 * @param bufsize   Size of buffer in bytes.
 * @return Result of the real tud_msc_scsi_cb (bytes of response data, or negative on error).
 * @note USB task context.
 */
int32_t __wrap_tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer,
                               uint16_t bufsize) {
    MscBotTap::onNonDataScsiEnter();
    return __real_tud_msc_scsi_cb(lun, scsi_cmd, buffer, bufsize);
}

/**
 * @brief Linker wrap of tud_msc_start_stop_cb: tap the CBW, then forward to the real callback.
 * @param lun              Logical unit number.
 * @param power_condition  START STOP UNIT power condition field.
 * @param start            START bit.
 * @param load_eject       LOEJ bit.
 * @return Result of the real tud_msc_start_stop_cb.
 * @note USB task context.
 */
bool __wrap_tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                                  bool load_eject) {
    MscBotTap::onNonDataScsiEnter();
    return __real_tud_msc_start_stop_cb(lun, power_condition, start, load_eject);
}

/**
 * @brief Linker wrap of mscd_control_xfer_cb: queue the SETUP and detect MSC BOT_RESET.
 *
 * A class request 0xFF (Bulk-Only Mass Storage Reset) at SETUP stage starts a new BOT epoch
 * after the real handler ran.
 *
 * @param rhport   Root hub port.
 * @param stage    Control stage (only CONTROL_STAGE_SETUP is tapped).
 * @param request  Control request; may be nullptr.
 * @return Result of the real mscd_control_xfer_cb.
 * @note USB task context.
 */
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

void __real_tud_mount_cb(void);
void __real_tud_umount_cb(void);
void __real_dcd_set_address(uint8_t rhport, uint8_t dev_addr);
void __real_usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr);
void __real_usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr);
bool __real_usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer,
                           uint16_t total_bytes);

/**
 * @brief Pad a 4-byte MODE SENSE(6) IN response to the host-requested CBW length.
 *
 * TinyUSB 0.15 builtin MODE_SENSE6 returns only the 4-byte header. BMW CBWs ask
 * for 192 bytes → Case 5 (Hi > Di) → usbd_edpt_stall(ep_in) before CSW → host
 * CLEAR_HALT + retry (ms6/cap≈2). Pad the IN buffer to the CBW length so residue
 * is 0 and the Case-5 stall never fires.
 *
 * Only acts on IN transfers of exactly 4 bytes whose current CBW is MODE SENSE(6) (0x1A)
 * with 4 < dCBWDataTransferLength <= 4096. Zero-fills the tail, patches the mode data length,
 * adds an empty Caching page header if room allows, and rewrites mscd total_len in `_mscd_itf`.
 *
 * @param ep_addr      Endpoint address (bit 7 set = IN).
 * @param[in,out] buffer  Transfer buffer; must hold dCBWDataTransferLength bytes.
 * @param total_bytes  Transfer length requested by TinyUSB.
 * @return New transfer length (the CBW length) or total_bytes unchanged.
 * @warning Writes into TinyUSB's private interface state at fixed 0.15 offsets.
 */
static uint16_t expandModeSense6IfNeeded(uint8_t ep_addr, uint8_t* buffer,
                                         uint16_t total_bytes) {
    if (!buffer || (ep_addr & 0x80) == 0 || total_bytes != 4) return total_bytes;
    const uint8_t* cbw = itfCbw();
    if (readLe32(cbw) != MSC_CBW_SIGNATURE) return total_bytes;
    if (cbw[15] != 0x1a) return total_bytes;  // SCSI MODE_SENSE(6)
    uint32_t expect = readLe32(cbw + 8);
    if (expect <= 4 || expect > 4096) return total_bytes;
    memset(buffer + 4, 0, (size_t)(expect - 4));
    buffer[0] = (uint8_t)(expect - 1);  // mode data length (bytes following)
    // Keep medium_type / WP bit from TinyUSB; no block descriptors (byte 3 = 0).
    // Optional Caching page — many hosts accept zeros-only after the header.
    if (expect >= 12) {
        buffer[4] = 0x08;  // page code: Caching
        buffer[5] = 0x0A;  // page length
    }
    // Keep mscd total_len in sync so DATA-complete compares xferred >= total_len.
    uint8_t* itf = _mscd_itf;
    itf[52] = (uint8_t)(expect);
    itf[53] = (uint8_t)(expect >> 8);
    itf[54] = (uint8_t)(expect >> 16);
    itf[55] = (uint8_t)(expect >> 24);
    return (uint16_t)expect;
}

/**
 * @brief Linker wrap of tud_mount_cb: queue the mount epoch before the real callback.
 * @note USB task context.
 */
void __wrap_tud_mount_cb(void) {
    // Queue before real so Capture-tUs ≤ any Arduino STARTED side effects; identity/phase
    // are emitted in drain with the same tUs (no cross-task interleave).
    MscBotTap::queueMount();
    __real_tud_mount_cb();
}

/**
 * @brief Linker wrap of tud_umount_cb: queue the umount epoch before the real callback.
 * @note USB task context.
 */
void __wrap_tud_umount_cb(void) {
    MscBotTap::queueUmount();
    __real_tud_umount_cb();
}

/**
 * @brief Linker wrap of dcd_set_address: queue a SET_ADDRESS event, then apply it.
 * @param rhport    Root hub port.
 * @param dev_addr  Device address assigned by the host.
 * @note USB task context.
 */
void __wrap_dcd_set_address(uint8_t rhport, uint8_t dev_addr) {
    MscBotTap::queueSetAddress(dev_addr);
    __real_dcd_set_address(rhport, dev_addr);
}

/**
 * @brief Linker wrap of usbd_edpt_stall: queue an endpoint STALL event, then stall.
 * @param rhport   Root hub port.
 * @param ep_addr  Endpoint address (bit 7 set = IN).
 * @note USB task context.
 */
void __wrap_usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr) {
    MscBotTap::queueEpStall(ep_addr);
    __real_usbd_edpt_stall(rhport, ep_addr);
}

/**
 * @brief Linker wrap of usbd_edpt_clear_stall: queue a Clear-Halt event, then clear the stall.
 * @param rhport   Root hub port.
 * @param ep_addr  Endpoint address (bit 7 set = IN).
 * @note USB task context.
 */
void __wrap_usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) {
    MscBotTap::queueEpClearHalt(ep_addr);
    __real_usbd_edpt_clear_stall(rhport, ep_addr);
}

/**
 * @brief Linker wrap of usbd_edpt_xfer: apply the MODE SENSE(6) padding, then start the transfer.
 * @param rhport       Root hub port.
 * @param ep_addr      Endpoint address (bit 7 set = IN).
 * @param buffer       Transfer buffer.
 * @param total_bytes  Requested transfer length in bytes.
 * @return Result of the real usbd_edpt_xfer.
 * @note USB task context.
 * @see expandModeSense6IfNeeded
 */
bool __wrap_usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t* buffer,
                           uint16_t total_bytes) {
    total_bytes = expandModeSense6IfNeeded(ep_addr, buffer, total_bytes);
    return __real_usbd_edpt_xfer(rhport, ep_addr, buffer, total_bytes);
}

/**
 * @brief TinyUSB callback after a READ10 data phase completed: queue the CSW.
 * @param lun  Logical unit number (unused).
 * @note USB task context.
 */
void tud_msc_read10_complete_cb(uint8_t lun) {
    (void)lun;
    MscBotTap::emitCsw();
}

/**
 * @brief TinyUSB callback after a WRITE10 data phase completed: queue the CSW.
 * @param lun  Logical unit number (unused).
 * @note USB task context.
 */
void tud_msc_write10_complete_cb(uint8_t lun) {
    (void)lun;
    MscBotTap::emitCsw();
}

/**
 * @brief TinyUSB callback after a generic SCSI command completed: queue the CSW.
 * @param lun       Logical unit number (unused).
 * @param scsi_cmd  16-byte SCSI CDB (unused).
 * @note USB task context.
 */
void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]) {
    (void)lun;
    (void)scsi_cmd;
    MscBotTap::emitCsw();
}

}  // extern "C"

#else

void MscBotTap::begin(ReadInstrument*) {}
ReadInstrument* MscBotTap::instrument() { return nullptr; }
void MscBotTap::setProbeSerial(const char*) {}
void MscBotTap::emitCbwIfNew(int64_t) {}
void MscBotTap::emitDataChunk(int64_t, uint32_t, uint32_t, uint32_t, uint32_t, const char*,
                              const char*, const char*) {}
void MscBotTap::emitCsw() {}
void MscBotTap::emitNeedResetIfNew() {}
void MscBotTap::onBotReset() {}
void MscBotTap::onNonDataScsiEnter() {}
void MscBotTap::afterMscdXfer() {}
void MscBotTap::poll() {}
void MscBotTap::drain(int) {}
void MscBotTap::queue(const Pending&) {}
void MscBotTap::queueControl(uint8_t, uint8_t, uint16_t, uint16_t, uint16_t) {}
void MscBotTap::queueSetAddress(uint8_t) {}
void MscBotTap::queueMount() {}
void MscBotTap::queueUmount() {}
void MscBotTap::queueEpStall(uint8_t) {}
void MscBotTap::queueEpClearHalt(uint8_t) {}
void MscBotTap::clearBotEpoch() {}
uint32_t MscBotTap::currentHostTag() { return 0; }
uint32_t MscBotTap::currentHostXferLen() { return 0; }

ReadInstrument* MscBotTap::instrument_ = nullptr;
uint32_t MscBotTap::lastCbwTag_ = 0;
bool MscBotTap::cswPending_ = false;
bool MscBotTap::needResetArmed_ = false;
int64_t MscBotTap::lastMountTUs_ = 0;
int64_t MscBotTap::lastUmountTUs_ = 0;
char MscBotTap::probeSerial_[16] = "HP0001";
MscBotTap::Pending MscBotTap::pending_[kPendingRing];
volatile size_t MscBotTap::pendingHead_ = 0;
volatile size_t MscBotTap::pendingTail_ = 0;
volatile size_t MscBotTap::pendingCount_ = 0;
uint32_t MscBotTap::pendingOverflow_ = 0;

#endif

volatile bool MscBotTap::uaPending_ = false;
volatile uint32_t MscBotTap::uaSent_ = 0;
volatile int64_t MscBotTap::uaSentUs_ = 0;

bool MscBotTap::consumeUnitAttention() {
    if (!uaPending_) return false;
    uaPending_ = false;
    uaSentUs_ = ProbeClock::nowUs();
    uaSent_ = uaSent_ + 1;
    return true;
}
