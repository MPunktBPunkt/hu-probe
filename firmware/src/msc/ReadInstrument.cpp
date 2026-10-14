#include "ReadInstrument.h"
#include "core/ProbeClock.h"
#include "net/EventBus.h"
#include <cstring>

void ReadInstrument::begin(Session* session) {
    session_ = session;
    windowStartMs_ = millis();
    lastDataEndUs_ = 0;
}

void ReadInstrument::stamp(JsonDocument& doc) {
    stampAt(doc, ProbeClock::nowUs());
}

void ReadInstrument::stampAt(JsonDocument& doc, int64_t tUs) {
    doc["v"] = 2;
    if (session_) {
        doc["sid"] = session_->id();
        doc["seq"] = session_->nextSeq();
    }
    // ArduinoJson stores as JSON number; safe within double mantissa for session lengths.
    doc["t_us"] = (long long)tUs;
    doc["t_ms"] = ProbeClock::nowMsCompat(tUs);
}

void ReadInstrument::onRead(uint32_t lba, uint32_t nBytes, uint32_t durUs,
                            const char* tag, const char* kind) {
    int64_t now = ProbeClock::nowUs();
    uint32_t gapUs = 0;
    if (lastDataEndUs_ > 0 && now >= lastDataEndUs_) {
        gapUs = (uint32_t)(now - lastDataEndUs_);
    }
    onReadTimed(now, lba, nBytes, durUs, gapUs, tag, kind);
    lastDataEndUs_ = now + (int64_t)durUs;
}

void ReadInstrument::onReadTimed(int64_t tUs, uint32_t lba, uint32_t nBytes, uint32_t durUs,
                                 uint32_t gapUs, const char* tag, const char* kind) {
    readCount_++;
    windowBytes_ += nBytes;
    if (!session_ || !session_->active()) return;

    JsonDocument doc;
    // Preserve MSC-callback time from the sample ring (not drain/loop time).
    stampAt(doc, tUs);
    doc["ev"] = "read";
    doc["lba"] = lba;
    doc["n"] = nBytes;
    doc["dur_us"] = durUs;
    doc["gap_us"] = gapUs;
    if (tag) doc["tag"] = tag;
    if (kind) doc["kind"] = kind;
    EventBus::instance().publish(doc);
}

void ReadInstrument::onScsi(const char* op) {
    if (op && strcmp(op, "tur") == 0) turCountWindow_++;
    if (op && strcmp(op, "write10") == 0) writeCount_++;
    if (!session_ || !session_->active()) return;

    JsonDocument doc;
    stamp(doc);
    doc["ev"] = "scsi";
    doc["legacy"] = true;
    doc["op"] = op ? op : "other";
    EventBus::instance().publish(doc);
}

void ReadInstrument::onUsb(const char* usbEv, uint8_t ep, const char* dir, uint32_t durUs,
                           const char* note) {
    if (!session_ || !session_->active()) return;
    JsonDocument doc;
    stamp(doc);
    doc["ev"] = "usb";
    doc["usb_ev"] = usbEv ? usbEv : "other";
    doc["ep"] = ep;
    if (dir) doc["dir"] = dir;
    doc["dur_us"] = durUs;
    if (note) doc["note"] = note;
    EventBus::instance().publish(doc);
}

void ReadInstrument::onBotCbw(uint32_t botTag, const char* cdbHex, const char* opcode,
                              uint32_t lba, uint16_t blocks, uint32_t xferLen, uint8_t flags) {
    if (opcode && strcmp(opcode, "tur") == 0) turCountWindow_++;
    if (opcode && strcmp(opcode, "write10") == 0) writeCount_++;
    if (!session_ || !session_->active()) return;

    JsonDocument doc;
    stamp(doc);
    doc["ev"] = "bot_cbw";
    doc["bot_tag"] = botTag;
    doc["xfer_len"] = xferLen;
    doc["flags"] = flags;
    doc["lun"] = 0;
    if (cdbHex) {
        doc["cdb_hex"] = cdbHex;
        doc["cdb_len"] = (int)(strlen(cdbHex) / 2);
    }
    JsonObject scsi = doc["scsi"].to<JsonObject>();
    if (opcode) scsi["opcode"] = opcode;
    scsi["lba"] = lba;
    scsi["blocks"] = blocks;
    EventBus::instance().publish(doc);
}

void ReadInstrument::onBotData(uint32_t botTag, const char* dir, uint32_t expected, uint32_t actual,
                               uint32_t durUs, uint32_t lba, const char* tag, const char* kind) {
    readCount_++;
    windowBytes_ += actual;
    int64_t now = ProbeClock::nowUs();
    uint32_t gapUs = 0;
    if (lastDataEndUs_ > 0 && now >= lastDataEndUs_) {
        gapUs = (uint32_t)(now - lastDataEndUs_);
    }
    lastDataEndUs_ = now + (int64_t)durUs;

    if (!session_ || !session_->active()) return;

    JsonDocument doc;
    stamp(doc);
    doc["ev"] = "bot_data";
    doc["bot_tag"] = botTag;
    if (dir) doc["dir"] = dir;
    doc["expected"] = expected;
    doc["actual"] = actual;
    doc["dur_us"] = durUs;
    doc["gap_us"] = gapUs;
    doc["lba"] = lba;
    if (tag) doc["tag"] = tag;
    if (kind) doc["kind"] = kind;
    EventBus::instance().publish(doc);

    // Komfort-Aggregat parallel
    JsonDocument rd;
    stamp(rd);
    rd["ev"] = "read";
    rd["bot_tag"] = botTag;
    rd["lba"] = lba;
    rd["n"] = actual;
    rd["dur_us"] = durUs;
    rd["gap_us"] = gapUs;
    if (tag) rd["tag"] = tag;
    if (kind) rd["kind"] = kind;
    EventBus::instance().publish(rd);
}

void ReadInstrument::onBotCsw(uint32_t botTag, uint32_t residue, uint8_t status) {
    if (!session_ || !session_->active()) return;
    JsonDocument doc;
    stamp(doc);
    doc["ev"] = "bot_csw";
    doc["bot_tag"] = botTag;
    doc["residue"] = residue;
    doc["status"] = status;
    EventBus::instance().publish(doc);
}

void ReadInstrument::tick() {
    unsigned long now = millis();
    unsigned long dt = now - windowStartMs_;
    if (dt >= 1000) {
        readRateKbS_ = (windowBytes_ / 1024.0f) * (1000.0f / (float)dt);
        turHz_ = turCountWindow_ * (1000.0f / (float)dt);
        windowBytes_ = 0;
        turCountWindow_ = 0;
        windowStartMs_ = now;
    }
}
