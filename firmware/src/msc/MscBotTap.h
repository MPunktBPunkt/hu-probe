#pragma once

#include <Arduino.h>
#include "ReadInstrument.h"

/**
 * GATE-01B: snapshot TinyUSB MSC BOT state (`_mscd_itf.cbw/csw`) at callback time.
 * Epistemic: host CBW/CDB/CSW bytes as seen by TinyUSB device stack — not wire packets.
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

    /** Non-data SCSI path (TUR / INQUIRY / …): CBW now, CSW via poll/complete. */
    static void onNonDataScsiEnter();
    /** Call from MscGadget::loop — catches CSW after builtin SCSI callbacks. */
    static void poll();

    static bool realBotEnabled() { return true; }
    static uint32_t currentHostTag();

private:
    static ReadInstrument* instrument_;
    static uint32_t lastCbwTag_;
    static bool cswPending_;
};
