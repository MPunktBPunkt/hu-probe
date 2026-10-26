/**
 * @file MscGadget.cpp
 * @brief MscGadget implementation on Arduino USBMSC/TinyUSB (stubs when ARDUINO_USB_MODE=1).
 */
#include "MscGadget.h"
#include "core/DiagLog.h"
#include "MscBotTap.h"
#include "ReadStall.h"
#include "BuildFlags.h"
#include "capture/AutoMarker.h"
#include "capture/OfflineCapture.h"
#include "media/ProbeMedia.h"
#include "core/PanicTail.h"
#include "core/ProbeClock.h"
#include "USB.h"
#include "USBMSC.h"
#include <cstring>

bool MscGadget::parseNotify(const char* s, MediaNotify& out) {
    if (!s || !s[0] || strcmp(s, "none") == 0) out = MediaNotify::None;
    else if (strcmp(s, "unit_attention") == 0 || strcmp(s, "ua") == 0) out = MediaNotify::UnitAttention;
    else if (strcmp(s, "remount") == 0) out = MediaNotify::Remount;
    else if (strcmp(s, "cycle") == 0) out = MediaNotify::Cycle;
    else return false;
    return true;
}

const char* MscGadget::notifyName(MediaNotify n) {
    switch (n) {
        case MediaNotify::UnitAttention: return "unit_attention";
        case MediaNotify::Remount: return "remount";
        case MediaNotify::Cycle: return "cycle";
        default: return "none";
    }
}

uint32_t MscGadget::mutationDueInMs() const {
    if (!mutPending_) return 0;
    const int32_t d = (int32_t)(mutDueMs_ - millis());
    return d > 0 ? (uint32_t)d : 0;
}

void MscGadget::mark(const char* name, const char* note, int64_t tUs) {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.capturing()) {
        oc.appendAutoMarker(tUs ? tUs : ProbeClock::nowUs(), name);
        return;
    }
    if (session_ && session_->active()) session_->marker(name, note ? note : "", tUs);
}

#if !ARDUINO_USB_MODE

#include "tusb.h"

/** @brief Gadget instance receiving the C-style USB callbacks; set in MscGadget::begin(). */
static MscGadget* g_msc = nullptr;

/** @brief Arduino USBMSC device backing the single LUN. */
static USBMSC MSC;

/**
 * @brief TinyUSB callback: report the LUN as read-only.
 * @param lun  Logical unit number (unused).
 * @return Always false (write-protected).
 * @note USB task context.
 */
extern "C" bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return false;
}

/**
 * @brief USBMSC read callback forwarding to MscGadget::onRead().
 * @param lba      Logical block address.
 * @param offset   Byte offset within the command.
 * @param buffer   Destination buffer.
 * @param bufsize  Requested bytes.
 * @return Result of MscGadget::onRead(), or -1 before begin().
 * @note USB task context.
 */
static int32_t huprobe_msc_read(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!g_msc) return -1;
    return g_msc->onRead(lba, offset, buffer, bufsize);
}

/**
 * @brief USBMSC write callback forwarding to MscGadget::onWrite().
 * @param lba      Logical block address.
 * @param offset   Byte offset within the command.
 * @param buffer   Host data.
 * @param bufsize  Chunk size in bytes.
 * @return Result of MscGadget::onWrite() (always -1), or -1 before begin().
 * @note USB task context.
 */
static int32_t huprobe_msc_write(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    if (!g_msc) return -1;
    return g_msc->onWrite(lba, offset, buffer, bufsize);
}

/**
 * @brief Arduino USB event handler mapping STARTED/STOPPED/SUSPEND/RESUME to the gadget.
 * @param arg         Handler argument (unused).
 * @param event_base  Event base; only ARDUINO_USB_EVENTS is handled.
 * @param event_id    ARDUINO_USB_*_EVENT id.
 * @param event_data  Event payload (unused).
 */
static void usb_event_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    (void)arg;
    (void)event_data;
    if (event_base != ARDUINO_USB_EVENTS || !g_msc) return;
    switch (event_id) {
        case ARDUINO_USB_STARTED_EVENT: g_msc->onUsbPlugged(true); break;
        case ARDUINO_USB_STOPPED_EVENT: g_msc->onUsbPlugged(false); break;
        case ARDUINO_USB_SUSPEND_EVENT: g_msc->onUsbSuspend(true); break;
        case ARDUINO_USB_RESUME_EVENT: g_msc->onUsbSuspend(false); break;
        default: break;
    }
}

void MscGadget::begin(Session* session, ReadInstrument* instrument, bool startDetached) {
    session_ = session;
    instrument_ = instrument;
    g_msc = this;
    remountGen_ = 1;
    snprintf(serial_, sizeof(serial_), "HP%04u", (unsigned)remountGen_);
    ProbeMedia& media = ProbeMedia::instance();
    media.begin();
    applyMediaIdentity();
    MscBotTap::begin(instrument);
    MscBotTap::setProbeSerial(serial_);

    USB.onEvent(usb_event_cb);
    MSC.vendorID("HUPROBE");
    MSC.productID("HU_PROBE");
    MSC.productRevision("0200");
    USB.serialNumber(serial_);
    MSC.onRead(huprobe_msc_read);
    MSC.onWrite(huprobe_msc_write);

    // Active medium (demo image or SynthFat variant) is the whole LUN.
    MSC.mediaPresent(false);
    if (!MSC.begin(media.sectorCount(), 512)) {
        DiagLog::printf("[MSC] begin failed");
        return;
    }
    USB.begin();
    ready_ = true;
    softAttached_ = true;
    if (startDetached && tud_disconnect()) softAttached_ = false;
    presentMedia("boot");
    DiagLog::printf("[MSC] ready media=%s sectors=%u serial=%s (soft USB cycle OK)\n",
                  media.variantId(), (unsigned)media.sectorCount(), serial_);
}

void MscGadget::applyMediaIdentity() {
    ProbeMedia& media = ProbeMedia::instance();
    const char* ser = media.usbSerial();
    fixedSerial_ = ser && ser[0];
    const char* lab = media.layout().label;
    setIdentity(fixedSerial_ ? ser : nullptr, (lab && lab[0]) ? lab : nullptr);
    MscBotTap::setProbeSerial(serial_);
}

bool MscGadget::switchMedia(const char* presetId, const String* specJson, bool usbCycle,
                            String* err, bool persist) {
    if (!ready_) {
        if (err) *err = "msc_not_ready";
        return false;
    }
    // Node indices of a pending mutation / UA belong to the old layout.
    mutPending_ = false;
    MscBotTap::cancelUnitAttention();
    ReadStall::instance().disable("media_switch");
    // LUN off while rebuilding. Full USB disconnect only for cycle=true (re-enum);
    // remount keeps the link so WiFi/OTG coexistence stays up on the lab host.
    const bool wasPresented = mediaPresented_;
    MSC.mediaPresent(false);
    mediaPresented_ = false;
    remountHoldUntilMs_ = 0;
    softReattachAtMs_ = 0;
    if (usbCycle && softAttached_) {
        tud_disconnect();
        softAttached_ = false;
        MscBotTap::clearNeedReset();
        delay(80);
    } else {
        delay(50);
    }
    yield();
    PanicTail::breadcrumb(PanicTail::MediaSwitch, 1);
    ProbeMedia& media = ProbeMedia::instance();
    const bool ok = specJson ? media.selectSpecJson(*specJson, err, persist)
                             : media.selectPreset(presetId, err, persist);
    if (!ok) {
        PanicTail::breadcrumb(PanicTail::MediaSwitch, 0x10);
        if (usbCycle && !softAttached_) softAttach();
        if (wasPresented) presentMedia("media_switch_failed");
        return false;
    }
    PanicTail::breadcrumb(PanicTail::MediaSwitch, 2);
    MSC.begin(media.sectorCount(), 512);
    applyMediaIdentity();
    lastReadEndUs_ = 0;
    if (session_ && session_->active()) {
        // One live session = one medium, so session.media / media_layout stay valid.
        session_->setPhase("media_switch", media.variantId());
        const String vehicle = session_->vehicle();
        const String scenario = session_->scenario();
        const String profile = session_->profileId();
        const String transport = session_->transport();
        session_->start(vehicle, scenario, profile, serial_, label_, transport.c_str());
    }
    DiagLog::printf("[MSC] media switch -> %s sectors=%u cycle=%d\n", media.variantId(),
                  (unsigned)media.sectorCount(), usbCycle ? 1 : 0);
    if (usbCycle) {
        presentMedia("media_switch");
        softReattachAtMs_ = millis() + 1500;
    } else {
        remountHoldUntilMs_ = millis() + 1500;
    }
    PanicTail::breadcrumb(PanicTail::MediaSwitch, 4);
    return true;
}

void MscGadget::loop() {
    if (!ready_) return;
    if (softReattachAtMs_ && (int32_t)(millis() - softReattachAtMs_) >= 0) {
        softReattachAtMs_ = 0;
        softAttach();
    }
    if (!mediaPresented_ && remountHoldUntilMs_ &&
        (int32_t)(millis() - remountHoldUntilMs_) >= 0) {
        remountHoldUntilMs_ = 0;
        presentMedia("remount");
    }
    if (mutPending_ && (int32_t)(millis() - mutDueMs_) >= 0) {
        mutPending_ = false;
        String err;
        if (!applyMutationNow(pendingMut_, pendingNotify_, &err)) {
            DiagLog::printf("[MSC] scheduled media mutate failed: %s\n", err.c_str());
        }
    }
    MscBotTap::poll();
    pollUnitAttention();
    ReadStall::instance().poll(session_);
    drainSamples();
}

bool MscGadget::mutateMedia(const ProbeMedia::Mutation& m, MediaNotify notify, uint32_t delayMs,
                            String* err) {
    if (!ready_) {
        if (err) *err = "msc_not_ready";
        return false;
    }
    if (delayMs == 0) {
        mutPending_ = false;
        return applyMutationNow(m, notify, err);
    }
    if (delayMs > 600000u) delayMs = 600000u;
    pendingMut_ = m;
    pendingNotify_ = notify;
    mutDueMs_ = millis() + delayMs;
    mutPending_ = true;
    char d[48];
    ProbeMedia::describeMutation(m, d, sizeof(d));
    DiagLog::printf("[MSC] media mutate in %ums (%s notify=%s)\n", (unsigned)delayMs, d,
                  notifyName(notify));
    return true;
}

bool MscGadget::applyMutationNow(const ProbeMedia::Mutation& m, MediaNotify notify, String* err) {
    ProbeMedia& media = ProbeMedia::instance();
    if (!media.applyMutation(m, err)) return false;
    char d[48];
    ProbeMedia::describeMutation(m, d, sizeof(d));
    char note[96];
    snprintf(note, sizeof(note), "%s notify=%s gen=%u", d, notifyName(notify),
             (unsigned)media.mutationGen());
    mark("media_mutate", note);
    DiagLog::printf("[MSC] media mutate %s\n", note);
    return notifyMedia(notify);
}

bool MscGadget::notifyMedia(MediaNotify n) {
    if (!ready_) return false;
    switch (n) {
        case MediaNotify::UnitAttention:
            MscBotTap::requestUnitAttention();
            mark("ua_request", mediaPresented_ ? "" : "media_hidden");
            return true;
        case MediaNotify::Remount:
            remountMedia(false);
            return true;
        case MediaNotify::Cycle:
            return softCycle(600);
        default:
            return true;
    }
}

void MscGadget::pollUnitAttention() {
    const uint32_t sent = MscBotTap::unitAttentionSent();
    if (sent == uaSeen_) return;
    uaSeen_ = sent;
    char note[24];
    snprintf(note, sizeof(note), "n=%u", (unsigned)sent);
    mark("ua_sent", note, MscBotTap::lastUnitAttentionUs());
    DiagLog::printf("[MSC] UNIT ATTENTION 06/28/00 sent (%s)\n", note);
}

void MscGadget::presentMedia(const char* reason) {
    if (mediaPresented_) return;
    MSC.mediaPresent(true);
    mediaPresented_ = true;
    AutoMarker::onMediaPresent(ProbeClock::nowUs());
    DiagLog::printf("[MSC] mediaPresent=true (%s)\n", reason ? reason : "");
}

void MscGadget::bumpSerial() {
    remountGen_++;
    if (remountGen_ == 0) remountGen_ = 1;
    snprintf(serial_, sizeof(serial_), "HP%04u", (unsigned)(remountGen_ % 10000));
    USB.serialNumber(serial_);
    MscBotTap::setProbeSerial(serial_);
}

void MscGadget::setIdentity(const char* serial, const char* label) {
    if (serial) {
        strncpy(serial_, serial, sizeof(serial_) - 1);
        serial_[sizeof(serial_) - 1] = 0;
        USB.serialNumber(serial_);
    }
    if (label) {
        strncpy(label_, label, sizeof(label_) - 1);
        label_[sizeof(label_) - 1] = 0;
    }
}

bool MscGadget::softDetach(uint16_t holdMs) {
    if (!ready_) return false;
    bool ok = tud_disconnect();
    if (ok) {
        softAttached_ = false;
        ReadStall::instance().onUsbReset();
        // Heal NEED_RESET latch + TinyUSB stage (API cycle / BOOT soft-replug / trip upload).
        MscBotTap::clearNeedReset();
        if (holdMs) {
            if (holdMs > 5000) holdMs = 5000;
            softReattachAtMs_ = millis() + holdMs;
        } else {
            // hold=0 must cancel a pending softCycle reattach (lab policy).
            softReattachAtMs_ = 0;
        }
        if (instrument_ && session_ && session_->active()) {
            instrument_->onUsb("detach", 0, "out", 0, "soft");
        }
        DiagLog::printf("[MSC] soft detach hold=%ums\n", (unsigned)holdMs);
    }
    return ok;
}

bool MscGadget::softAttach() {
    if (!ready_) return false;
    bool ok = tud_connect();
    if (ok) {
        softAttached_ = true;
        transportEpoch_++;
        softReattachAtMs_ = 0;
        if (instrument_ && session_ && session_->active()) {
            instrument_->onUsb("attach", 0, "out", 0, "soft");
        }
        DiagLog::printf("[MSC] soft attach");
    }
    return ok;
}

bool MscGadget::softCycle(uint16_t holdMs) {
    if (holdMs < 50) holdMs = 50;
    return softDetach(holdMs);
}

bool MscGadget::hideMedia() {
    if (!ready_ || !mediaPresented_) return ready_;
    MSC.mediaPresent(false);
    mediaPresented_ = false;
    remountHoldUntilMs_ = 0;
    AutoMarker::onMediaHide(ProbeClock::nowUs());
    DiagLog::printf("[MSC] media hidden");
    return true;
}

bool MscGadget::restoreMedia() {
    if (!ready_) return false;
    presentMedia("offline_restore");
    return mediaPresented_;
}

void MscGadget::remountMedia(bool bumpIdentity) {
    if (!ready_) return;
    if (bumpIdentity) bumpSerial();
    MSC.mediaPresent(false);
    mediaPresented_ = false;
    remountHoldUntilMs_ = millis() + 600;
    // Flush deferred Capture-tUs BOT before remount phase stamp.
    MscBotTap::drain(128);
    if (session_ && session_->active()) {
        session_->setPhase("remount", bumpIdentity ? serial_ : "keepId");
    }
    DiagLog::printf("[MSC] media remount ser=%s\n", serial_);
}

void MscGadget::onUsbPlugged(bool on) {
    if (plugged_ == on) return;
    plugged_ = on;
    suspended_ = false;
    ReadStall::instance().onUsbReset();
    if (on) {
        plugCount_++;
        transportEpoch_++;
        // mount/identity/phase come from MscBotTap::drain(Kind::Mount) — same Capture-tUs.
        DiagLog::printf("[MSC] USB plugged (host started / mount)");
    } else {
        unplugCount_++;
        // Next attach looks like a new stick — unless the media variant pins the serial.
        if (fixedSerial_) {
            applyMediaIdentity();
        } else if (!pinnedSerial_) {
            bumpSerial();
        }
        // umount/phase from MscBotTap::drain(Kind::Umount).
        DiagLog::printf("[MSC] USB unplugged — next serial %s\n", serial_);
    }
}

void MscGadget::onUsbSuspend(bool on) {
    if (!plugged_ || suspended_ == on) return;
    suspended_ = on;
    if (instrument_ && session_ && session_->active()) {
        instrument_->onUsb(on ? "suspend" : "resume", 0, "in", 0, "host");
    }
}

void MscGadget::tagForLba(uint32_t lba, char* tag, size_t n, char* kind, size_t kn) const {
    // B21 (0.2.31): bands come from the active medium's FAT layout, not hardcoded LBAs.
    const MediaLayout::Region r = ProbeMedia::instance().classify(lba, nullptr, nullptr);
    snprintf(tag, n, "%s", MediaLayout::regionTag(r));
    snprintf(kind, kn, "%s", MediaLayout::regionKind(r));
}

int32_t MscGadget::onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    ProbeMedia& media = ProbeMedia::instance();
    if (lba >= media.sectorCount()) return -1;
    if (bufsize > 4096) bufsize = 4096;
    uint8_t* out = (uint8_t*)buffer;

    // CBW at its arrival time, so CBW -> first bot_data spans a held (BUSY) command.
    if (offset == 0) MscBotTap::emitCbwIfNew(ProbeClock::nowUs());
    if (ReadStall::instance().hold(lba)) return 0;

    int64_t t0 = ProbeClock::nowUs();
    media.read(lba, offset, out, bufsize);
    int64_t t1 = ProbeClock::nowUs();
    uint32_t dur = 0;
    if (t1 >= t0) {
        int64_t d = t1 - t0;
        dur = (d > 0xFFFFFFFFLL) ? 0xFFFFFFFFu : (uint32_t)d;
    }
    uint32_t gap = 0;
    if (lastReadEndUs_ > 0 && t0 >= lastReadEndUs_) {
        int64_t g = t0 - lastReadEndUs_;
        gap = (g > 0xFFFFFFFFLL) ? 0xFFFFFFFFu : (uint32_t)g;
    }
    lastReadEndUs_ = t1;
    readCount_++;

    char tag[8];
    char kind[8];
    tagForLba(lba, tag, sizeof(tag), kind, sizeof(kind));

    // Comfort read is emitted with bot_data in MscBotTap::drain.
    MscBotTap::emitDataChunk(t0, lba, bufsize, dur, gap, tag, kind, "in");
    return (int32_t)bufsize;
}

int32_t MscGadget::onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void)lba;
    (void)offset;
    (void)buffer;
    Sample s;
        s.tUs = ProbeClock::nowUs();
    s.lba = lba;
    s.n = bufsize;
    s.scsi = 1;
    snprintf(s.tag, sizeof(s.tag), "WR");
    snprintf(s.kind, sizeof(s.kind), "write");
    pushSample(s);
    return -1;  // write-protected
}

void MscGadget::pushSample(const Sample& s) {
    if (count_ >= kSampleRing) {
        head_ = (head_ + 1) % kSampleRing;
        count_--;
        sampleOverflow_++;
    }
    ring_[tail_] = s;
    tail_ = (tail_ + 1) % kSampleRing;
    count_++;
}

void MscGadget::drainSamples() {
    // Bound work per loop tick (lab: 4k bursts need faster drain than status spam).
    for (int i = 0; i < 128 && count_ > 0; i++) {
        Sample s = ring_[head_];
        head_ = (head_ + 1) % kSampleRing;
        count_--;
        if (!instrument_) continue;
        if (s.scsi == 1) {
            instrument_->onScsi("write10");
        } else if (MscBotTap::realBotEnabled()) {
            // 0.2.12+: comfort read emitted with bot_data in MscBotTap::drain (same Capture-tUs).
        } else {
            instrument_->emitBotRead10Synth(s.tUs, s.lba, s.n, s.durUs, s.gapUs, s.tag, s.kind);
        }
    }
}

#else  // ARDUINO_USB_MODE

void MscGadget::begin(Session*, ReadInstrument*, bool) {
    DiagLog::printf("[MSC] disabled (ARDUINO_USB_MODE=1)");
}
void MscGadget::loop() {}
void MscGadget::setIdentity(const char* serial, const char* label) {
    if (serial) {
        strncpy(serial_, serial, sizeof(serial_) - 1);
    }
    if (label) {
        strncpy(label_, label, sizeof(label_) - 1);
    }
}
void MscGadget::applyMediaIdentity() {}
bool MscGadget::switchMedia(const char*, const String*, bool, String* err, bool) {
    if (err) *err = "msc_disabled";
    return false;
}
bool MscGadget::mutateMedia(const ProbeMedia::Mutation&, MediaNotify, uint32_t, String* err) {
    if (err) *err = "msc_disabled";
    return false;
}
bool MscGadget::applyMutationNow(const ProbeMedia::Mutation&, MediaNotify, String*) { return false; }
bool MscGadget::notifyMedia(MediaNotify) { return false; }
void MscGadget::pollUnitAttention() {}
bool MscGadget::softDetach(uint16_t) { return false; }
bool MscGadget::softAttach() { return false; }
bool MscGadget::softCycle(uint16_t) { return false; }
void MscGadget::remountMedia(bool) {}
bool MscGadget::hideMedia() { return false; }
bool MscGadget::restoreMedia() { return false; }
void MscGadget::onUsbPlugged(bool) {}
void MscGadget::onUsbSuspend(bool) {}
int32_t MscGadget::onRead(uint32_t, uint32_t, void*, uint32_t) { return -1; }
int32_t MscGadget::onWrite(uint32_t, uint32_t, uint8_t*, uint32_t) { return -1; }
void MscGadget::presentMedia(const char*) {}
void MscGadget::bumpSerial() {}
void MscGadget::pushSample(const Sample&) {}
void MscGadget::drainSamples() {}
void MscGadget::tagForLba(uint32_t, char*, size_t, char*, size_t) const {}

#endif
