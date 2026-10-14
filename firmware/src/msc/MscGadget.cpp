#include "MscGadget.h"
#include "MscBotTap.h"
#include "DemoFatImage.h"
#include "BuildFlags.h"
#include "core/ProbeClock.h"
#include "USB.h"
#include "USBMSC.h"
#include <cstring>

#if !ARDUINO_USB_MODE

#include "tusb.h"

static MscGadget* g_msc = nullptr;
static USBMSC MSC;

extern "C" bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return false;
}

static int32_t huprobe_msc_read(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!g_msc) return -1;
    return g_msc->onRead(lba, offset, buffer, bufsize);
}

static int32_t huprobe_msc_write(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    if (!g_msc) return -1;
    return g_msc->onWrite(lba, offset, buffer, bufsize);
}

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

void MscGadget::begin(Session* session, ReadInstrument* instrument) {
    session_ = session;
    instrument_ = instrument;
    g_msc = this;
    remountGen_ = 1;
    snprintf(serial_, sizeof(serial_), "HP%04u", (unsigned)remountGen_);
    MscBotTap::begin(instrument);
    MscBotTap::setProbeSerial(serial_);

    USB.onEvent(usb_event_cb);
    MSC.vendorID("HUPROBE");
    MSC.productID("HU_PROBE");
    MSC.productRevision("0200");
    USB.serialNumber(serial_);
    MSC.onRead(huprobe_msc_read);
    MSC.onWrite(huprobe_msc_write);

    // First field stick: serve the known demo FAT image as the whole LUN.
    MSC.mediaPresent(false);
    if (!MSC.begin(DEMO_FAT_SECTOR_COUNT, DEMO_FAT_SECTOR_SIZE)) {
        Serial.println("[MSC] begin failed");
        return;
    }
    USB.begin();
    ready_ = true;
    softAttached_ = true;
    presentMedia("boot");
    Serial.printf("[MSC] ready sectors=%u serial=%s (soft USB cycle OK)\n",
                  DEMO_FAT_SECTOR_COUNT, serial_);
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
    MscBotTap::poll();
    drainSamples();
}

void MscGadget::presentMedia(const char* reason) {
    if (mediaPresented_) return;
    MSC.mediaPresent(true);
    mediaPresented_ = true;
    Serial.printf("[MSC] mediaPresent=true (%s)\n", reason ? reason : "");
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
        if (holdMs) {
            if (holdMs > 5000) holdMs = 5000;
            softReattachAtMs_ = millis() + holdMs;
        }
        if (instrument_ && session_ && session_->active()) {
            instrument_->onUsb("detach", 0, "out", 0, "soft");
        }
        Serial.printf("[MSC] soft detach hold=%ums\n", (unsigned)holdMs);
    }
    return ok;
}

bool MscGadget::softAttach() {
    if (!ready_) return false;
    bool ok = tud_connect();
    if (ok) {
        softAttached_ = true;
        softReattachAtMs_ = 0;
        if (instrument_ && session_ && session_->active()) {
            instrument_->onUsb("attach", 0, "out", 0, "soft");
        }
        Serial.println("[MSC] soft attach");
    }
    return ok;
}

bool MscGadget::softCycle(uint16_t holdMs) {
    if (holdMs < 50) holdMs = 50;
    return softDetach(holdMs);
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
    Serial.printf("[MSC] media remount ser=%s\n", serial_);
}

void MscGadget::onUsbPlugged(bool on) {
    if (plugged_ == on) return;
    plugged_ = on;
    suspended_ = false;
    if (on) {
        plugCount_++;
        // mount/identity/phase come from MscBotTap::drain(Kind::Mount) — same Capture-tUs.
        Serial.println("[MSC] USB plugged (host started / mount)");
    } else {
        unplugCount_++;
        bumpSerial();  // next attach looks like a new stick
        // umount/phase from MscBotTap::drain(Kind::Umount).
        Serial.printf("[MSC] USB unplugged — next serial %s\n", serial_);
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
    if (lba == 0) {
        snprintf(tag, n, "BOOT");
        snprintf(kind, kn, "boot");
    } else if (lba >= 1 && lba <= 16) {
        snprintf(tag, n, "FAT");
        snprintf(kind, kn, "fat");
    } else if (lba >= 17 && lba <= 48) {
        snprintf(tag, n, "ROOT");
        snprintf(kind, kn, "dir");
    } else {
        snprintf(tag, n, "DATA");
        snprintf(kind, kn, "file");
    }
}

int32_t MscGadget::onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (lba >= DEMO_FAT_SECTOR_COUNT) return -1;
    if (bufsize > 4096) bufsize = 4096;
    uint8_t* out = (uint8_t*)buffer;

    int64_t t0 = ProbeClock::nowUs();
    uint32_t pos = lba * DEMO_FAT_SECTOR_SIZE + offset;
    if (pos >= DEMO_FAT_SIZE) {
        memset(out, 0, bufsize);
    } else {
        uint32_t avail = DEMO_FAT_SIZE - pos;
        uint32_t n = bufsize;
        if (n > avail) n = avail;
        memcpy(out, DEMO_FAT_IMAGE + pos, n);
        if (n < bufsize) memset(out + n, 0, bufsize - n);
    }
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

    // GATE-01B: CBW + DATA share Capture-tUs so drain stays non-decreasing (0.2.12).
    if (offset == 0) {
        MscBotTap::emitCbwIfNew(t0);
    }
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

void MscGadget::begin(Session*, ReadInstrument*) {
    Serial.println("[MSC] disabled (ARDUINO_USB_MODE=1)");
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
bool MscGadget::softDetach(uint16_t) { return false; }
bool MscGadget::softAttach() { return false; }
bool MscGadget::softCycle(uint16_t) { return false; }
void MscGadget::remountMedia(bool) {}
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
