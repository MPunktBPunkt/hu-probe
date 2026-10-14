#pragma once

#include <Arduino.h>
#include "ReadInstrument.h"

/**
 * GATE-01B: TinyUSB MSC BOT tap — primary via `--wrap=mscd_xfer_cb` (0.2.9),
 * fallback snapshot of `_mscd_itf` at app callback time (0.2.8).
 * Epistemic: host CBW/CDB/CSW as seen by TinyUSB device stack — not wire packets.
 */
class MscBotTap {
public:
    static void begin(ReadInstrument* instrument);
    static ReadInstrument* instrument();

    /** Emit bot_cbw from current TinyUSB CBW (call once per command, e.g. offset==0). */
    static void emitCbwIfNew();
    /** Emit bot_data + comfort read for a DATA chunk (READ10/WRITE10). */
    static void emitDataChunk(uint32_t lba, uint32_t nBytes, uint32_t durUs, uint32_t gapUs,
                              const char* tag, const char* kind, const char* dir);
    /** Emit bot_csw from current TinyUSB CSW. */
    static void emitCsw();
    /** Emit usb bot_need_reset when TinyUSB enters MSC_STAGE_NEED_RESET. */
    static void emitNeedResetIfNew();
    /** MSC BOT_RESET recovered the class driver — allow a later need_reset event. */
    static void onBotReset();

    /** Non-data SCSI path (TUR / INQUIRY / …): CBW now, CSW via poll/complete. */
    static void onNonDataScsiEnter();
    /** After mscd_xfer_cb: CBW / CSW / NEED_RESET from stage. */
    static void afterMscdXfer();
    /** Call from MscGadget::loop — catches CSW after builtin SCSI callbacks. */
    static void poll();

    static bool realBotEnabled() { return true; }
    static uint32_t currentHostTag();
    /** True after NEED_RESET until BOT_RESET / begin — for StatusLed. */
    static bool needResetLatched() { return needResetArmed_; }

private:
    static ReadInstrument* instrument_;
    static uint32_t lastCbwTag_;
    static bool cswPending_;
    static bool needResetArmed_;
};
