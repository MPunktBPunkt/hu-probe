#pragma once

/**
 * @file MscGadget.h
 * @brief TinyUSB mass-storage gadget: LUN presentation, soft USB attach/detach, live media changes.
 */

#include <Arduino.h>
#include "session/Session.h"
#include "media/ProbeMedia.h"
#include "msc/ReadInstrument.h"

/**
 * @brief Minimal TinyUSB MSC gadget for first HU field tests.
 *
 * Soft USB cycle = tud_disconnect/connect (D+ pull-up), not VBUS.
 */
class MscGadget {
public:
    /**
     * @brief Initialise ProbeMedia, MscBotTap and the Arduino USBMSC device, then start USB.
     *
     * The medium is presented immediately ("boot"); on USBMSC begin failure the gadget stays
     * not ready.
     *
     * @param session        Live session for markers/phases; may be nullptr.
     * @param instrument     Event sink for BOT/USB events; may be nullptr.
     * @param startDetached  D+ off right after USB.begin (drive mode: attach after the boot check).
     */
    void begin(Session* session, ReadInstrument* instrument, bool startDetached = false);

    /**
     * @brief Run timers and deferred work: auto-reattach, remount end, scheduled mutation,
     *        MscBotTap::poll(), UNIT ATTENTION markers, ReadStall::poll() and write-note drain.
     * @note Arduino loop task only.
     */
    void loop();

    /** @brief True once begin() completed successfully. */
    bool ready() const { return ready_; }

    /** @brief True while the host has started the USB device (between STARTED and STOPPED events). */
    bool plugged() const { return plugged_; }

    /** @brief True while the host has suspended the bus. */
    bool suspended() const { return suspended_; }

    /** @brief True while the LUN reports the medium as present. */
    bool mediaPresented() const { return mediaPresented_; }

    /** @brief True while the D+ pull-up is connected (not soft-detached). */
    bool usbSoftAttached() const { return softAttached_; }

    /** @brief Counter incremented on every plug event and soft attach. */
    uint32_t transportEpoch() const { return transportEpoch_; }

    /** @brief USB serial number currently reported to the host. */
    const char* identitySerial() const { return serial_; }

    /** @brief Volume label of the active medium identity. */
    const char* volumeLabel() const { return label_; }

    /**
     * @brief Set the USB serial and/or volume label.
     * @param serial  New USB serial (max 15 chars, also applied to USB.serialNumber); nullptr keeps the current one.
     * @param label   New volume label (max 11 chars); nullptr keeps the current one.
     */
    void setIdentity(const char* serial, const char* label);

    /**
     * @brief Soft bus detach (+ optional auto-reattach).
     *
     * Disconnects the D+ pull-up and resets ReadStall. holdMs = 0 cancels a pending
     * softCycle reattach (lab policy).
     *
     * @param holdMs  Reattach after this many ms (clamped to 5000); 0 = stay detached.
     * @return true if tud_disconnect() succeeded, false if not ready or it failed.
     */
    bool softDetach(uint16_t holdMs = 0);

    /**
     * @brief Reconnect the D+ pull-up and bump the transport epoch.
     * @return true if tud_connect() succeeded, false if not ready or it failed.
     */
    bool softAttach();

    /**
     * @brief Soft bus cycle (detach → hold → attach).
     * @param holdMs  Detached time in ms (clamped to 50..5000); reattach happens in loop().
     * @return Result of softDetach().
     */
    bool softCycle(uint16_t holdMs = 600);

    /**
     * @brief SCSI media not-ready window then present again (≠ bus cycle).
     *
     * Hides the LUN for 600 ms (re-presented in loop()), flushes up to 128 pending BOT events
     * and sets the session phase "remount".
     *
     * @param bumpIdentity  true = switch to a new USB serial first.
     */
    void remountMedia(bool bumpIdentity = true);

    /**
     * @brief Hide only the LUN while keeping the USB transport attached.
     * @return true if hidden now or already hidden, false if the gadget is not ready.
     */
    bool hideMedia();

    /**
     * @brief Restore the LUN hidden by hideMedia().
     * @return true if the medium is presented afterwards, false if the gadget is not ready.
     */
    bool restoreMedia();

    /**
     * @brief Switch the active medium to a preset or a custom spec.
     *
     * 0.2.31: switch ProbeMedia to a preset (presetId) or custom spec (specJson).
     * LUN hidden while rebuilding; usbCycle=true re-enumerates (host re-reads
     * capacity/FS), else NOT READY window then present.
     * Cancels a pending mutation/UNIT ATTENTION, disables ReadStall and restarts an active
     * session with the new identity. Blocks for 50-80 ms.
     *
     * @param presetId  Preset id, used when specJson is nullptr.
     * @param specJson  Custom media spec JSON; takes precedence over presetId.
     * @param usbCycle  true = soft detach and re-enumerate, false = NOT READY window only.
     * @param[out] err  Error text on failure (e.g. "msc_not_ready"); may be nullptr.
     * @param persist   Store the selection in NVS (forwarded to ProbeMedia::selectPreset() / selectSpecJson()).
     * @return true if the new medium is active, false on error (previous presentation restored).
     * @note Arduino loop task.
     */
    bool switchMedia(const char* presetId, const String* specJson, bool usbCycle, String* err,
                     bool persist = true);

    /** @brief How the host is told about a live content change. */
    enum class MediaNotify : uint8_t { None, UnitAttention, Remount, Cycle };

    /**
     * @brief Parse a notify mode name.
     * @param s         "none" (or nullptr/empty), "unit_attention" or "ua", "remount", "cycle".
     * @param[out] out  Parsed mode; unchanged on failure.
     * @return true if s was recognised, false otherwise.
     */
    static bool parseNotify(const char* s, MediaNotify& out);

    /**
     * @brief Return the canonical name of a notify mode.
     * @param n  Notify mode.
     * @return "unit_attention", "remount", "cycle" or "none" (static string).
     */
    static const char* notifyName(MediaNotify n);

    /**
     * @brief Apply a live file change now or after a delay, then notify the host.
     *
     * Live file change (ProbeMedia::Mutation) now or after delayMs, then notify.
     * Markers: media_mutate (+ ua_request / ua_sent). Allowed during offline capture.
     * A new request replaces a still pending one.
     *
     * @param m         Mutation to apply (copied when scheduled).
     * @param notify    How the host is told about the change.
     * @param delayMs   Delay in ms (clamped to 600000); 0 = apply immediately.
     * @param[out] err  Error text on failure; may be nullptr.
     * @return true if applied or scheduled, false if not ready or the immediate apply failed.
     */
    bool mutateMedia(const ProbeMedia::Mutation& m, MediaNotify notify, uint32_t delayMs,
                     String* err);

    /**
     * @brief Notify only (no content change): UNIT ATTENTION, NOT READY remount or bus cycle.
     * @param n  Notify mode; Remount keeps the identity, Cycle holds 600 ms.
     * @return false if not ready or the soft cycle failed, true otherwise.
     */
    bool notifyMedia(MediaNotify n);

    /** @brief True while a delayed mutateMedia() request waits to be applied. */
    bool mutationPending() const { return mutPending_; }

    /**
     * @brief Return the time until the pending mutation is applied.
     * @return Remaining time in ms, 0 if none is pending or it is already due.
     */
    uint32_t mutationDueInMs() const;

    /**
     * @brief Keep the current serial across unplug/re-enum (SEQ-CACHE: cache key = content).
     * @param on  true = pin the serial, false = bump it on every unplug.
     */
    void setSerialPinned(bool on) { pinnedSerial_ = on; }

    /** @brief True if the serial is pinned across unplug (see setSerialPinned()). */
    bool serialPinned() const { return pinnedSerial_; }

    /**
     * @brief Handle the Arduino USB STARTED/STOPPED event.
     *
     * Updates plug counters and resets ReadStall. On unplug the next serial is chosen
     * (media-pinned serial, pinned serial or bumped serial). Mount/umount events themselves
     * come from MscBotTap.
     *
     * @param on  true = host started the device, false = stopped.
     * @note Called from the Arduino USB event callback.
     */
    void onUsbPlugged(bool on);

    /**
     * @brief Handle the Arduino USB SUSPEND/RESUME event and report it to the instrument.
     * @param on  true = suspended, false = resumed; ignored while unplugged.
     * @note Called from the Arduino USB event callback.
     */
    void onUsbSuspend(bool on);

    /**
     * @brief Serve a READ10 chunk from ProbeMedia and queue its bot_data event.
     *
     * Queues the CBW at offset 0 so CBW -> first bot_data spans a held command; ReadStall
     * may hold the read.
     *
     * @param lba      Logical block address.
     * @param offset   Byte offset within the block sequence of this command.
     * @param buffer   Destination buffer.
     * @param bufsize  Requested bytes (clamped to 4096).
     * @return Bytes served, 0 while ReadStall holds the read (TinyUSB retries), -1 if lba is out of range.
     * @note USB task context (USBMSC onRead callback).
     */
    int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);

    /**
     * @brief Reject a WRITE10 chunk and record a write note.
     * @param lba      Logical block address.
     * @param offset   Byte offset (unused).
     * @param buffer   Host data (unused).
     * @param bufsize  Chunk size in bytes.
     * @return Always -1 (write-protected).
     * @note USB task context (USBMSC onWrite callback).
     */
    int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);

    /** @brief Number of USB plug (host STARTED) events since boot. */
    uint32_t plugCount() const { return plugCount_; }

    /** @brief Number of USB unplug (host STOPPED) events since boot. */
    uint32_t unplugCount() const { return unplugCount_; }

    /** @brief Number of READ10 chunks served since boot. */
    uint32_t readCount() const { return readCount_; }

    /** @brief Number of write notes dropped because the 32-entry sample ring was full. */
    uint32_t sampleOverflow() const { return sampleOverflow_; }

private:
    /** @brief Write note captured in the USB task and drained in loop(). */
    struct Sample {
        int64_t tUs = 0;  // MSC-callback capture time (must survive drain)
        uint32_t lba = 0;
        uint32_t n = 0;
        uint32_t durUs = 0;  // full µs — no uint16 cap (lab: gap hit 65535)
        uint32_t gapUs = 0;
        uint32_t botTag = 0;  // host CBW tag (TinyUSB)
        char tag[8] = {0};
        char kind[8] = {0};
        uint8_t scsi = 0;  // 0=read, 1=write note
    };

    /**
     * @brief Present the medium on the LUN (no-op if already presented).
     * @param reason  Log/diagnostic reason such as "boot" or "remount"; may be nullptr.
     */
    void presentMedia(const char* reason);

    /** @brief Advance the remount generation and switch to serial "HP%04u" (USB and MscBotTap). */
    void bumpSerial();

    /** @brief Serial/label from the active media variant (usb_serial pins the serial). */
    void applyMediaIdentity();

    /**
     * @brief Append a write note to the sample ring, dropping the oldest if full.
     * @param s  Sample to copy.
     * @note USB task context.
     */
    void pushSample(const Sample& s);

    /**
     * @brief Publish up to 128 queued samples (write notes as onScsi("write10")).
     * @note Arduino loop task.
     */
    void drainSamples();

    /**
     * @brief Classify an LBA by the active medium's FAT layout into region tag and kind.
     * @param lba        Logical block address.
     * @param[out] tag   Region tag buffer.
     * @param n          Size of tag in bytes.
     * @param[out] kind  Region kind buffer.
     * @param kn         Size of kind in bytes.
     */
    void tagForLba(uint32_t lba, char* tag, size_t n, char* kind, size_t kn) const;

    /**
     * @brief Apply a mutation to ProbeMedia, set the media_mutate marker and notify the host.
     * @param m         Mutation to apply.
     * @param notify    How the host is told about the change.
     * @param[out] err  Error text on failure; may be nullptr.
     * @return false if ProbeMedia rejected the mutation or notifyMedia() failed, true otherwise.
     */
    bool applyMutationNow(const ProbeMedia::Mutation& m, MediaNotify notify, String* err);

    /**
     * @brief Record a marker in the offline capture (if capturing) or the active session.
     * @param name  Marker name.
     * @param note  Marker note; nullptr = empty (ignored for offline capture).
     * @param tUs   Capture time in µs (ProbeClock); 0 = now.
     */
    void mark(const char* name, const char* note, int64_t tUs = 0);

    /** @brief Emit a ua_sent marker when MscBotTap reports a newly sent UNIT ATTENTION. */
    void pollUnitAttention();

    bool mutPending_ = false;
    ProbeMedia::Mutation pendingMut_;
    MediaNotify pendingNotify_ = MediaNotify::None;
    uint32_t mutDueMs_ = 0;
    uint32_t uaSeen_ = 0;

    Session* session_ = nullptr;
    ReadInstrument* instrument_ = nullptr;
    bool ready_ = false;
    bool plugged_ = false;
    bool suspended_ = false;
    bool mediaPresented_ = false;
    bool softAttached_ = true;
    uint32_t remountHoldUntilMs_ = 0;
    uint32_t softReattachAtMs_ = 0;
    uint16_t remountGen_ = 1;
    char serial_[16] = "HP0001";
    char label_[12] = "HUPROBE";
    bool fixedSerial_ = false;
    bool pinnedSerial_ = false;
    uint32_t plugCount_ = 0;
    uint32_t unplugCount_ = 0;
    uint32_t readCount_ = 0;
    uint32_t transportEpoch_ = 0;
    int64_t lastReadEndUs_ = 0;

    // Only write notes use this ring since 0.2.12 (reads go through MscBotTap);
    // 32 instead of 512 frees ~25 KiB DRAM for the offline buffer and SynthFat.
    static constexpr size_t kSampleRing = 32;
    Sample ring_[kSampleRing];
    volatile size_t head_ = 0;
    volatile size_t tail_ = 0;
    volatile size_t count_ = 0;
    uint32_t sampleOverflow_ = 0;
};
