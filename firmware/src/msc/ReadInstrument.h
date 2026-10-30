#pragma once

/**
 * @file ReadInstrument.h
 * @brief Converts MSC/BOT/USB activity into versioned JSON events on the EventBus.
 */

#include <Arduino.h>
#include "session/Session.h"

/**
 * @brief LBA/timing/BOT hooks, fed by the MSC callback / sample ring.
 *
 * All emit/on* methods publish only while a session is active; counters (reads, writes,
 * TUR rate, throughput) are updated regardless where noted.
 */
class ReadInstrument {
public:
    /**
     * @brief Bind the session and reset the rate window, data gap tracking and synthetic BOT tag.
     * @param session  Session that provides sid/seq and receives phases; may be nullptr.
     */
    void begin(Session* session);

    /**
     * @brief Emit a synthetic READ10 chain stamped now, with the gap since the previous read.
     *
     * Comfort path (sim / aggregate). For real evidence use onBot* / emitBotRead10Synth.
     *
     * @param lba     First logical block.
     * @param nBytes  Bytes read.
     * @param durUs   Read duration in µs.
     * @param tag     Region tag; may be nullptr.
     * @param kind    Region kind; may be nullptr.
     */
    void onRead(uint32_t lba, uint32_t nBytes, uint32_t durUs, const char* tag, const char* kind);

    /**
     * @brief Emit a synthesized bot_cbw / bot_data / read / bot_csw chain for one READ10.
     *
     * Stage A fallback: synthetic BOT chain. From 0.2.8 the drain uses the TinyUSB tap (stage B).
     * Uses an internal incrementing BOT tag; the CSW is stamped at tUs + durUs.
     * Counts the read and its bytes even without an active session.
     *
     * @param tUs     Capture time in µs (ProbeClock).
     * @param lba     First logical block.
     * @param nBytes  Bytes read (blocks = ceil(nBytes / 512), at least 1).
     * @param durUs   Read duration in µs.
     * @param gapUs   Idle time since the previous read in µs.
     * @param tag     Region tag; may be nullptr.
     * @param kind    Region kind; may be nullptr.
     */
    void emitBotRead10Synth(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs,
                            uint32_t gapUs, const char* tag, const char* kind);

    /**
     * @brief Emit a legacy "scsi" event and count "tur" / "write10".
     *
     * Deprecated short form — stub/sim only. Still used by MscGadget for write notes.
     *
     * @param op  SCSI op name such as "tur" or "write10"; nullptr = "other".
     */
    void onScsi(const char* op);

    /**
     * @brief Emit a generic "usb" event stamped now.
     * @param usbEv  USB event name (e.g. "attach", "detach", "suspend"); nullptr = "other".
     * @param ep     Endpoint number.
     * @param dir    Direction "in"/"out"; may be nullptr.
     * @param durUs  Duration in µs.
     * @param note   Optional note; may be nullptr.
     */
    void onUsb(const char* usbEv, uint8_t ep, const char* dir, uint32_t durUs,
               const char* note = nullptr);

    /**
     * @brief Emit the gadget identity stamped now.
     *
     * Known gadget identity as usb get_descriptor timeline (no host control sniff yet).
     *
     * @param serial    USB serial; may be nullptr.
     * @param vendor    Vendor string; may be nullptr.
     * @param product   Product string; may be nullptr.
     * @param revision  Revision string; may be nullptr.
     * @see emitIdentityDescriptorsAt
     */
    void emitIdentityDescriptors(const char* serial, const char* vendor, const char* product,
                                 const char* revision);

    /**
     * @brief Emit the gadget identity as two synthesized get_descriptor events (device, string).
     * @param tUs       Capture time in µs (ProbeClock).
     * @param serial    USB serial; may be nullptr.
     * @param vendor    Vendor string; may be nullptr.
     * @param product   Product string; may be nullptr.
     * @param revision  Firmware/revision string; may be nullptr.
     */
    void emitIdentityDescriptorsAt(int64_t tUs, const char* serial, const char* vendor,
                                   const char* product, const char* revision);

    /**
     * @brief Emit a bot_cbw event from pre-decoded fields, stamped now.
     *
     * Counts "tur" and "write10" opcodes even without an active session. Used by the web UI simulator.
     *
     * @param botTag   dCBWTag.
     * @param cdbHex   CDB as hex string; cdb_len is derived from its length; may be nullptr.
     * @param opcode   SCSI opcode name; may be nullptr.
     * @param lba      Logical block address from the CDB.
     * @param blocks   Transfer length in blocks from the CDB.
     * @param xferLen  dCBWDataTransferLength in bytes.
     * @param flags    bmCBWFlags (0x80 = IN).
     */
    void onBotCbw(uint32_t botTag, const char* cdbHex, const char* opcode,
                  uint32_t lba, uint16_t blocks, uint32_t xferLen, uint8_t flags);

    /**
     * @brief Emit bot_data plus a comfort read event, stamped now.
     *
     * The gap is measured from the end of the previous data phase. Counts the read and its
     * bytes even without an active session. Used by the web UI simulator.
     *
     * @param botTag    dCBWTag of the command.
     * @param dir       Direction "in"/"out"; may be nullptr.
     * @param expected  Expected bytes.
     * @param actual    Transferred bytes.
     * @param durUs     Data phase duration in µs.
     * @param lba       First logical block.
     * @param tag       Region tag; may be nullptr.
     * @param kind      Region kind; may be nullptr.
     */
    void onBotData(uint32_t botTag, const char* dir, uint32_t expected, uint32_t actual,
                   uint32_t durUs, uint32_t lba, const char* tag, const char* kind);

    /**
     * @brief Emit a bot_csw event from decoded fields, stamped now.
     * @param botTag   dCSWTag.
     * @param residue  dCSWDataResidue in bytes.
     * @param status   bCSWStatus (0 = passed).
     */
    void onBotCsw(uint32_t botTag, uint32_t residue, uint8_t status);

    /**
     * @brief Decode and emit a real CBW as bot_cbw.
     *
     * GATE-01B: host CBW/CSW bytes from TinyUSB `_mscd_itf` (`synthesized=false`).
     * TUR updates the rate window and TUR period statistics; WRITE10 increments writeCount().
     * Ignored if the signature is not USBC.
     *
     * @param cbw  Raw 31-byte CBW; may be nullptr (ignored).
     * @param tUs  Capture time in µs (ProbeClock).
     * @note Arduino loop task (MscBotTap::drain() or offline replay).
     */
    void emitBotCbwTinyusb(const uint8_t cbw[31], int64_t tUs);

    /**
     * @brief Emit a real bot_data event followed by its comfort read.
     *
     * bot_data + comfort read at same Capture-tUs (keeps seq/t_us non-decreasing).
     * Adds file fields from the active medium and chunk info during coalesced replay.
     *
     * @param tUs     Capture time in µs (ProbeClock).
     * @param botTag  dCBWTag of the command.
     * @param lba     First logical block.
     * @param nBytes  Chunk length in bytes (expected = actual).
     * @param durUs   Chunk duration in µs.
     * @param gapUs   Idle time since the previous chunk in µs.
     * @param tag     Region tag; may be nullptr.
     * @param kind    Region kind; may be nullptr.
     * @param dir     Direction "in"/"out"; may be nullptr.
     */
    void emitBotDataTinyusb(int64_t tUs, uint32_t botTag, uint32_t lba, uint32_t nBytes,
                            uint32_t durUs, uint32_t gapUs, const char* tag, const char* kind,
                            const char* dir);

    /**
     * @brief Decode and emit a real CSW as bot_csw (ignored if the signature is not USBS).
     * @param csw  Raw 13-byte CSW; may be nullptr (ignored).
     * @param tUs  Capture time in µs (ProbeClock).
     */
    void emitBotCswTinyusb(const uint8_t csw[13], int64_t tUs);

    /**
     * @brief Emit a control SETUP as a "usb" event, classified as MSC class or standard request.
     * @param bmRequestType    SETUP bmRequestType (bit 7 = IN).
     * @param bRequest         SETUP bRequest.
     * @param wValue           SETUP wValue.
     * @param wIndex           SETUP wIndex.
     * @param wLength          SETUP wLength.
     * @param tUs              Capture time in µs (ProbeClock).
     * @param needResetActive  true if NEED_RESET was latched (CLEAR_FEATURE is noted as not healed).
     */
    void emitUsbControlTinyusb(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue,
                               uint16_t wIndex, uint16_t wLength, int64_t tUs,
                               bool needResetActive = false, uint8_t stage = 1);

    /**
     * @brief Emit usb bot_need_reset.
     *
     * TinyUSB MSC_STAGE_NEED_RESET (invalid CBW → stall both EPs).
     *
     * @param tUs  Capture time in µs (ProbeClock).
     */
    void emitBotNeedResetTinyusb(int64_t tUs);

    /**
     * @brief Emit a USB epoch event.
     *
     * Enum/STALL epoch: set_address, set_configuration, mount/umount, ep_stall/clear, bot_reset.
     *
     * @param tUs     Capture time in µs (ProbeClock).
     * @param usbEv   Event name such as "mount" or "ep_stall"; nullptr = nothing emitted.
     * @param ep      Endpoint address (0 if not applicable).
     * @param wValue  Event value (e.g. address or configuration).
     * @param note    Optional note; may be nullptr.
     */
    void emitUsbEpochTinyusb(int64_t tUs, const char* usbEv, uint8_t ep, uint16_t wValue,
                             const char* note);

    /**
     * @brief Set the session phase with an explicit capture time.
     * @param tUs    Capture time in µs (ProbeClock).
     * @param phase  Phase name such as "enum" or "idle"; nullptr = ignored.
     * @param note   Phase note; nullptr = empty.
     */
    void emitPhaseAt(int64_t tUs, const char* phase, const char* note);

    /**
     * @brief Emit only the comfort read event and count the read.
     *
     * Comfort `read` only (BOT chain already emitted as tinyusb).
     *
     * @param tUs     Capture time in µs (ProbeClock).
     * @param botTag  dCBWTag of the command.
     * @param lba     First logical block.
     * @param nBytes  Bytes read.
     * @param durUs   Read duration in µs.
     * @param gapUs   Idle time since the previous read in µs.
     * @param tag     Region tag; may be nullptr.
     * @param kind    Region kind; may be nullptr.
     */
    void emitReadAggregate(int64_t tUs, uint32_t botTag, uint32_t lba, uint32_t nBytes,
                           uint32_t durUs, uint32_t gapUs, const char* tag, const char* kind);

    /**
     * @brief Set the chunk count for the next replayed DATA events.
     *
     * Offline replay of a coalesced DATA record (n MSC chunks); 1 = normal.
     *
     * @param n  Number of coalesced MSC chunks; 0 is treated as 1.
     */
    void setReplayChunks(uint16_t n) { chunks_ = n ? n : 1; }

    /**
     * @brief Write TUR period statistics into a JSON object.
     *
     * TUR period statistics (Capture-tUs based; burst < 0.5 s, idle = 0.5…5 s).
     * Writes tur_count, tur_burst_n, tur_idle_n and, if idle samples exist,
     * tur_idle_p10_ms / tur_idle_median_ms / tur_idle_p90_ms over the last 64 idle periods.
     *
     * @param o  Target JSON object.
     */
    void turStatsJson(JsonObject o) const;

    /** @brief Clear TUR period statistics (also done automatically when the session id changes). */
    void resetTurStats();

    /** @brief Read throughput in KiB/s over the last window of at least 1 s (updated by tick()). */
    float readRateKbS() const { return readRateKbS_; }

    /** @brief TEST UNIT READY rate in Hz over the last window of at least 1 s (updated by tick()). */
    float turHz() const { return turHz_; }

    /** @brief Number of read events counted since boot. */
    uint32_t readCount() const { return readCount_; }

    /** @brief Number of WRITE10 commands/notes counted since boot. */
    uint32_t writeCount() const { return writeCount_; }

    /** @brief Last BOT tag used by the synthetic READ10 chain. */
    uint32_t botTag() const { return botTag_; }

    /**
     * @brief Recompute readRateKbS() and turHz() once at least 1000 ms have passed.
     * @note Arduino loop task.
     */
    void tick();

private:
    /**
     * @brief Stamp v/sid/seq/t_us/t_ms and origin fields with the current event time.
     * @param doc  Event document to stamp.
     */
    void stamp(JsonDocument& doc);

    /**
     * @brief Stamp v/sid/seq/t_us/t_ms and origin fields with a capture time.
     *
     * Late Capture-tUs (deferred BOT / phase with older mount stamp) must not walk
     * the timeline backwards, so tUs is passed through ProbeClock::publishUs(); the raw
     * value goes to the origin fields.
     *
     * @param doc  Event document to stamp.
     * @param tUs  Capture time in µs (ProbeClock).
     */
    void stampAt(JsonDocument& doc, int64_t tUs);

    /**
     * @brief Encode bytes as lowercase hex with NUL terminator.
     * @param in      Input bytes.
     * @param n       Number of input bytes.
     * @param[out] out  Output buffer; set to "" if smaller than 2*n+1.
     * @param outLen  Size of out in bytes.
     */
    void bytesToHex(const uint8_t* in, size_t n, char* out, size_t outLen);

    /**
     * @brief Add file / file_idx / file_blk from the active medium (B21).
     * @param doc  Event document to extend; unchanged if lba does not belong to a file node.
     * @param lba  Logical block address to classify.
     */
    void addFileFields(JsonDocument& doc, uint32_t lba);

    /**
     * @brief Classify a TUR by its distance to the previous one (burst < 500 ms, idle <= 5000 ms).
     *
     * Statistics are per session; a session id change resets them.
     *
     * @param tUs  Capture time of the TUR in µs (ProbeClock).
     */
    void noteTur(int64_t tUs);

    static constexpr uint8_t kTurIdleRing = 64;
    static constexpr uint32_t kTurBurstMs = 500;
    static constexpr uint32_t kTurIdleMaxMs = 5000;
    int64_t lastTurUs_ = 0;
    uint32_t turTotal_ = 0;
    uint32_t turBurst_ = 0;
    uint32_t turIdle_[kTurIdleRing] = {0};
    uint8_t turIdleHead_ = 0;
    uint8_t turIdleCount_ = 0;
    uint16_t chunks_ = 1;
    char turSid_[40] = "";

    Session* session_ = nullptr;
    int64_t lastDataEndUs_ = 0;
    unsigned long windowStartMs_ = 0;
    uint32_t windowBytes_ = 0;
    uint32_t turCountWindow_ = 0;
    float readRateKbS_ = 0;
    float turHz_ = 0;
    uint32_t readCount_ = 0;
    uint32_t writeCount_ = 0;
    uint32_t botTag_ = 0;
};
