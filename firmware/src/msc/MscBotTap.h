#pragma once

#include <Arduino.h>
#include "ReadInstrument.h"

/**
 * GATE-01B: TinyUSB MSC BOT tap — primary via `--wrap=mscd_xfer_cb` (0.2.9+),
 * fallback snapshot of `_mscd_itf` at app callback time.
 *
 * 0.2.11: callbacks only memcpy into a pending ring; JSON publish happens in
 * drain()/poll() on the Arduino loop — keeps WiFi/lwIP off the USB hot path.
 * 0.2.12: pending entries carry Capture-tUs; drain uses stampAt (not loop-now)
 * so bot_data/read stay non-decreasing with seq.
 * 0.2.13: Control/STALL epochs — wrap mount/umount, dcd_set_address, edpt stall/clear;
 * Clear-Halt never clears NEED_RESET; Mount clears BOT epoch (replug without VBUS).
 * Epistemic: device-stack bytes, not wire packets.
 */
class MscBotTap {
public:
    static void begin(ReadInstrument* instrument);
    static ReadInstrument* instrument();
    /** Soft identity string for mount-drain gadget_identity events. */
    static void setProbeSerial(const char* serial);

    /** Queue bot_cbw from current TinyUSB CBW (deduped); tUs = Capture time. */
    static void emitCbwIfNew(int64_t tUs);
    /** Queue bot_data for a DATA chunk (READ10/WRITE10); tUs = MSC-callback capture time. */
    static void emitDataChunk(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs,
                              uint32_t gapUs, const char* tag, const char* kind, const char* dir);
    /** Queue bot_csw from current TinyUSB CSW. */
    static void emitCsw();
    /** Queue usb bot_need_reset when TinyUSB enters MSC_STAGE_NEED_RESET. */
    static void emitNeedResetIfNew();
    /** MSC BOT_RESET recovered the class driver — new epoch (not Clear-Halt). */
    static void onBotReset();

    /** Non-data SCSI path (TUR / INQUIRY / …): CBW now, CSW via poll/complete. */
    static void onNonDataScsiEnter();
    /** After mscd_xfer_cb: CBW / CSW / NEED_RESET from stage. */
    static void afterMscdXfer();
    /** Call from MscGadget::loop — stage catch-up + drain pending JSON emits. */
    static void poll();
    /** Flush pending BOT/USB events to EventBus (bounded). */
    static void drain(int maxEvents = 64);
    /** Queue MSC/standard control SETUP (from wrap — no JSON in USB path). */
    static void queueControl(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue,
                             uint16_t wIndex, uint16_t wLength);

    /** 0.2.13 Control/Enum/STALL queue helpers (USB callbacks — no JSON). */
    static void queueSetAddress(uint8_t addr);
    static void queueMount();
    static void queueUmount();
    static void queueEpStall(uint8_t epAddr);
    static void queueEpClearHalt(uint8_t epAddr);

    static bool realBotEnabled() { return true; }
    static uint32_t currentHostTag();
    /** dCBWDataTransferLength of the current CBW (0 if none). */
    static uint32_t currentHostXferLen();
    /** True after NEED_RESET until BOT_RESET / Mount / begin — for StatusLed. */
    static bool needResetLatched() { return needResetArmed_; }
    static uint32_t pendingOverflow() { return pendingOverflow_; }
    /** Capture-tUs of last queueMount / queueUmount — align phase/identity stamps. */
    static int64_t lastMountCaptureUs() { return lastMountTUs_; }
    static int64_t lastUmountCaptureUs() { return lastUmountTUs_; }

    /**
     * Soft media change: the next TEST UNIT READY while the medium is present fails with
     * UNIT ATTENTION 06/28/00 ("not ready to ready change, medium may have changed").
     * The host learns it on its next TUR poll (NBT EVO idle: every ~2.24 s).
     */
    static void requestUnitAttention() { uaPending_ = true; }
    static void cancelUnitAttention() { uaPending_ = false; }
    static bool unitAttentionPending() { return uaPending_; }
    static uint32_t unitAttentionSent() { return uaSent_; }
    static int64_t lastUnitAttentionUs() { return uaSentUs_; }
    /** TUR wrap (USB task): true once per request. */
    static bool consumeUnitAttention();

private:
    enum class Kind : uint8_t {
        Cbw = 1,
        Data = 2,
        Csw = 3,
        NeedReset = 4,
        Control = 5,
        SetAddress = 6,
        Mount = 7,
        Umount = 8,
        EpStall = 9,
        EpClearHalt = 10,
        BotReset = 11
    };

    struct Pending {
        Kind kind = Kind::Cbw;
        int64_t tUs = 0;  // Capture time at USB/MSC callback (survives drain)
        uint8_t raw[31] = {0};
        uint32_t botTag = 0;
        uint32_t lba = 0;
        uint32_t nBytes = 0;
        uint32_t durUs = 0;
        uint32_t gapUs = 0;
        uint16_t wValue = 0;
        uint16_t wIndex = 0;
        uint16_t wLength = 0;
        uint8_t bmRequestType = 0;
        uint8_t bRequest = 0;
        uint8_t epAddr = 0;
        uint8_t flag = 0;  // e.g. clear_halt while need_reset
        char tag[8] = {0};
        char kindStr[8] = {0};
        char dir[4] = {0};
    };

    static void queue(const Pending& p);
    /** Drop BOT tag/CSW/NEED_RESET latch — Mount or BOT Reset epoch. */
    static void clearBotEpoch();

    static ReadInstrument* instrument_;
    static uint32_t lastCbwTag_;
    static bool cswPending_;
    static bool needResetArmed_;
    static int64_t lastMountTUs_;
    static int64_t lastUmountTUs_;
    static char probeSerial_[16];

    static constexpr size_t kPendingRing = 256;
    static Pending pending_[kPendingRing];
    static volatile size_t pendingHead_;
    static volatile size_t pendingTail_;
    static volatile size_t pendingCount_;
    static uint32_t pendingOverflow_;

    static volatile bool uaPending_;
    static volatile uint32_t uaSent_;
    static volatile int64_t uaSentUs_;
};
