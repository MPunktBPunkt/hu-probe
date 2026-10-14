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
 * Epistemic: device-stack bytes, not wire packets.
 */
class MscBotTap {
public:
    static void begin(ReadInstrument* instrument);
    static ReadInstrument* instrument();

    /** Queue bot_cbw from current TinyUSB CBW (deduped); tUs = Capture time. */
    static void emitCbwIfNew(int64_t tUs);
    /** Queue bot_data for a DATA chunk (READ10/WRITE10); tUs = MSC-callback capture time. */
    static void emitDataChunk(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs,
                              uint32_t gapUs, const char* tag, const char* kind, const char* dir);
    /** Queue bot_csw from current TinyUSB CSW. */
    static void emitCsw();
    /** Queue usb bot_need_reset when TinyUSB enters MSC_STAGE_NEED_RESET. */
    static void emitNeedResetIfNew();
    /** MSC BOT_RESET recovered the class driver — allow a later need_reset event. */
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

    static bool realBotEnabled() { return true; }
    static uint32_t currentHostTag();
    /** True after NEED_RESET until BOT_RESET / begin — for StatusLed. */
    static bool needResetLatched() { return needResetArmed_; }
    static uint32_t pendingOverflow() { return pendingOverflow_; }

private:
    enum class Kind : uint8_t { Cbw = 1, Data = 2, Csw = 3, NeedReset = 4, Control = 5 };

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
        char tag[8] = {0};
        char kindStr[8] = {0};
        char dir[4] = {0};
    };

    static void queue(const Pending& p);

    static ReadInstrument* instrument_;
    static uint32_t lastCbwTag_;
    static bool cswPending_;
    static bool needResetArmed_;

    static constexpr size_t kPendingRing = 256;
    static Pending pending_[kPendingRing];
    static volatile size_t pendingHead_;
    static volatile size_t pendingTail_;
    static volatile size_t pendingCount_;
    static uint32_t pendingOverflow_;
};
