#pragma once

/**
 * @file MscBotTap.h
 * @brief TinyUSB MSC Bulk-Only-Transport tap: captures CBW/DATA/CSW and USB control/epoch events.
 */

#include <Arduino.h>
#include "ReadInstrument.h"

/**
 * @brief Static tap that queues TinyUSB MSC BOT and USB epoch events and publishes them from the loop.
 *
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
    /**
     * @brief Attach the event sink and reset the BOT epoch and the pending ring.
     * @param instrument  Sink for published events; nullptr makes drain() discard everything.
     * @note Called from MscGadget::begin() before USB.begin().
     */
    static void begin(ReadInstrument* instrument);

    /** @brief Event sink set by begin() (nullptr before begin or without TinyUSB MSC). */
    static ReadInstrument* instrument();

    /**
     * @brief Set the probe serial reported in mount-time identity events.
     *
     * Soft identity string for mount-drain gadget_identity events.
     *
     * @param serial  NUL-terminated serial; truncated to 15 chars; nullptr is ignored.
     */
    static void setProbeSerial(const char* serial);

    /**
     * @brief Queue bot_cbw from the current TinyUSB CBW (deduped).
     *
     * Ignored if the CBW signature is invalid or the tag equals the last CBW while its CSW
     * is still pending. Marks a CSW as pending and clears the NEED_RESET latch.
     *
     * @param tUs  Capture time in µs (ProbeClock) at the USB/MSC callback.
     * @note USB task context; no JSON work, only a ring insert.
     */
    static void emitCbwIfNew(int64_t tUs);

    /**
     * @brief Queue bot_data for a DATA chunk (READ10/WRITE10).
     * @param tUs     MSC-callback capture time in µs (ProbeClock).
     * @param lba     First logical block of the chunk.
     * @param nBytes  Chunk length in bytes.
     * @param durUs   Time spent producing the chunk in µs.
     * @param gapUs   Idle time since the end of the previous read chunk in µs (0 if unknown).
     * @param tag     Region tag (copied, max 7 chars); may be nullptr.
     * @param kind    Region kind (copied, max 7 chars); may be nullptr.
     * @param dir     Direction string such as "in" (copied, max 3 chars); may be nullptr.
     * @note USB task context (called from MscGadget::onRead()); the host CBW tag is captured here.
     */
    static void emitDataChunk(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs,
                              uint32_t gapUs, const char* tag, const char* kind, const char* dir);

    /**
     * @brief Queue bot_csw from the current TinyUSB CSW.
     *
     * No-op unless a CSW is pending for the last CBW and the CSW signature is valid;
     * clears the pending flag afterwards.
     */
    static void emitCsw();

    /**
     * @brief Queue usb bot_need_reset when TinyUSB enters MSC_STAGE_NEED_RESET.
     *
     * Emitted once per latch; the latch stays set until BOT_RESET, Mount, a new CBW or begin().
     * Also writes a PanicTail breadcrumb.
     */
    static void emitNeedResetIfNew();

    /**
     * @brief Record an MSC BOT_RESET and start a new BOT epoch.
     *
     * MSC BOT_RESET recovered the class driver — new epoch (not Clear-Halt).
     *
     * @note USB task context (from the mscd_control_xfer_cb wrap).
     */
    static void onBotReset();

    /**
     * @brief Queue the CBW for a non-data SCSI command.
     *
     * Non-data SCSI path (TUR / INQUIRY / …): CBW now, CSW via poll/complete.
     *
     * @note USB task context (tud_msc_*_cb wraps).
     */
    static void onNonDataScsiEnter();

    /**
     * @brief Derive CBW / CSW / NEED_RESET events from the MSC stage after mscd_xfer_cb.
     * @note USB task context (__wrap_mscd_xfer_cb).
     */
    static void afterMscdXfer();

    /**
     * @brief Catch up on missed stage transitions, then drain up to 16 pending events.
     *
     * Call from MscGadget::loop — stage catch-up + drain pending JSON emits.
     *
     * @note Arduino loop task only.
     */
    static void poll();

    /**
     * @brief Flush pending BOT/USB events to EventBus (bounded).
     *
     * During an offline capture the events go to OfflineCapture instead of the instrument.
     * Yields every 8 events so WiFi/lwIP keeps running. Without an instrument the ring is dropped.
     *
     * @param maxEvents  Maximum number of events to publish in this call.
     * @note Arduino loop task only.
     */
    static void drain(int maxEvents = 64);

    /**
     * @brief Queue an MSC/standard control SETUP (from wrap — no JSON in USB path).
     *
     * A standard CLEAR_FEATURE while NEED_RESET is latched is flagged (logged, not healed).
     * Stage must be TinyUSB CONTROL_STAGE_* (SETUP=1); only SETUP samples are queued as events.
     *
     * @param bmRequestType  SETUP bmRequestType.
     * @param bRequest       SETUP bRequest.
     * @param wValue         SETUP wValue.
     * @param wIndex         SETUP wIndex.
     * @param wLength        SETUP wLength.
     * @param stage          TinyUSB control stage (CONTROL_STAGE_SETUP expected for publish).
     * @note USB task context.
     */
    static void queueControl(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue,
                             uint16_t wIndex, uint16_t wLength, uint8_t stage = 1);

    /** @brief CONTROL_STAGE_SETUP samples queued since begin(). */
    static uint32_t controlSetupCount() { return controlSetupCount_; }
    /** @brief Control-callback invocations with stage != SETUP (instrument sanity). */
    static uint32_t controlOtherStageCount() { return controlOtherStageCount_; }
    /** @brief True when the pending ring has not overflowed since begin(). */
    static bool captureValid() { return pendingOverflow_ == 0; }

    /**
     * @brief Queue a SET_ADDRESS epoch event.
     *
     * 0.2.13 Control/Enum/STALL queue helpers (USB callbacks — no JSON).
     *
     * @param addr  Device address assigned by the host.
     */
    static void queueSetAddress(uint8_t addr);

    /**
     * @brief Queue a mount epoch event and clear the BOT epoch.
     *
     * Mount is the safe epoch because replug without VBUS often skips umount.
     * Stores the capture time for lastMountCaptureUs(); drain() emits mount, identity and
     * phase with the same timestamp.
     *
     * @note USB task context (__wrap_tud_mount_cb).
     */
    static void queueMount();

    /**
     * @brief Queue an umount epoch event, then clear the BOT epoch.
     * @note USB task context (__wrap_tud_umount_cb).
     */
    static void queueUmount();

    /**
     * @brief Queue an endpoint STALL epoch event.
     * @param epAddr  Endpoint address (bit 7 set = IN).
     * @note USB task context (__wrap_usbd_edpt_stall).
     */
    static void queueEpStall(uint8_t epAddr);

    /**
     * @brief Queue an endpoint Clear-Halt epoch event.
     *
     * Clear-Halt never clears NEED_RESET; the event is flagged if NEED_RESET is latched.
     *
     * @param epAddr  Endpoint address (bit 7 set = IN).
     * @note USB task context (__wrap_usbd_edpt_clear_stall).
     */
    static void queueEpClearHalt(uint8_t epAddr);

    /** @brief Whether real TinyUSB BOT events replace synthesized READ10 events (always true). */
    static bool realBotEnabled() { return true; }

    /**
     * @brief Return the dCBWTag of the current CBW.
     * @return Tag of the CBW in TinyUSB's interface state, or the last seen tag if it is invalid.
     */
    static uint32_t currentHostTag();

    /**
     * @brief Return the dCBWDataTransferLength of the current CBW.
     * @return Host-requested transfer length in bytes, 0 if there is no valid CBW.
     */
    static uint32_t currentHostXferLen();

    /** @brief True after NEED_RESET until BOT_RESET / Mount / begin — for StatusLed. */
    static bool needResetLatched() { return needResetArmed_; }

    /**
     * @brief Clear a latched NEED_RESET after an intentional soft-detach / soft-cycle.
     *
     * Without this the magenta LED stays on and coex logic treats the bus as broken even
     * though we disconnected on purpose (trip upload WiFi TX, Lab `/api/usb/cycle`, BOOT
     * double soft-replug). Also resets TinyUSB `_mscd_itf.stage` when stuck in
     * MSC_STAGE_NEED_RESET so poll() cannot re-arm the latch across a D+ toggle that
     * skipped mscd_reset — otherwise Soft-Cycle appears to do nothing and the HU sees
     * no medium (BOT both-EP stall).
     */
    static void clearNeedReset();

    /** @brief Number of pending events dropped because the 256-entry ring was full. */
    static uint32_t pendingOverflow() { return pendingOverflow_; }

    /** @brief Capture-tUs of last queueMount — align phase/identity stamps. */
    static int64_t lastMountCaptureUs() { return lastMountTUs_; }

    /** @brief Capture-tUs of last queueUmount — align phase/identity stamps. */
    static int64_t lastUmountCaptureUs() { return lastUmountTUs_; }

    /**
     * @brief Request a soft media change notification via UNIT ATTENTION.
     *
     * Soft media change: the next TEST UNIT READY while the medium is present fails with
     * UNIT ATTENTION 06/28/00 ("not ready to ready change, medium may have changed").
     * The host learns it on its next TUR poll (NBT EVO idle: every ~2.24 s).
     */
    static void requestUnitAttention() { uaPending_ = true; }

    /** @brief Drop a pending UNIT ATTENTION request that has not been sent yet. */
    static void cancelUnitAttention() { uaPending_ = false; }

    /** @brief True while a requested UNIT ATTENTION waits for the next successful TUR. */
    static bool unitAttentionPending() { return uaPending_; }

    /** @brief Number of UNIT ATTENTION responses sent since boot. */
    static uint32_t unitAttentionSent() { return uaSent_; }

    /** @brief ProbeClock time in µs of the last UNIT ATTENTION sent (0 if none). */
    static int64_t lastUnitAttentionUs() { return uaSentUs_; }

    /**
     * @brief Consume a pending UNIT ATTENTION request.
     *
     * TUR wrap (USB task): true once per request. Updates unitAttentionSent() and
     * lastUnitAttentionUs().
     *
     * @return true if a request was pending (caller must fail the TUR with 06/28/00), false otherwise.
     */
    static bool consumeUnitAttention();

private:
    /** @brief Type of a pending event in the ring. */
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

    /** @brief One queued event, captured in the USB task and published later by drain(). */
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
        uint8_t stage = 0;  // TinyUSB CONTROL_STAGE_* for Kind::Control
        uint8_t epAddr = 0;
        uint8_t flag = 0;  // e.g. clear_halt while need_reset
        char tag[8] = {0};
        char kindStr[8] = {0};
        char dir[4] = {0};
    };

    /**
     * @brief Append an event to the pending ring, dropping the oldest entry if full.
     *
     * Raises the ProbeClock event watermark to p.tUs so status/health cannot stamp below it.
     *
     * @param p  Event to copy into the ring.
     */
    static void queue(const Pending& p);

    /** @brief Drop BOT tag/CSW/NEED_RESET latch — Mount or BOT Reset epoch. */
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
    static uint32_t controlSetupCount_;
    static uint32_t controlOtherStageCount_;

    static volatile bool uaPending_;
    static volatile uint32_t uaSent_;
    static volatile int64_t uaSentUs_;
};
