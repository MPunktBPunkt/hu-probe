#include "ReadInstrument.h"
#include "core/ProbeClock.h"
#include "net/EventBus.h"
#include <cstring>

void ReadInstrument::begin(Session* session) {
    session_ = session;
    windowStartMs_ = millis();
    lastDataEndUs_ = 0;
    botTag_ = 0;
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
    doc["t_us"] = (long long)tUs;
    doc["t_ms"] = ProbeClock::nowMsCompat(tUs);
}

void ReadInstrument::bytesToHex(const uint8_t* in, size_t n, char* out, size_t outLen) {
    static const char* kHex = "0123456789abcdef";
    size_t need = n * 2 + 1;
    if (outLen < need) {
        if (outLen) out[0] = 0;
        return;
    }
    for (size_t i = 0; i < n; i++) {
        out[i * 2] = kHex[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = kHex[in[i] & 0xF];
    }
    out[n * 2] = 0;
}

void ReadInstrument::onRead(uint32_t lba, uint32_t nBytes, uint32_t durUs,
                            const char* tag, const char* kind) {
    int64_t now = ProbeClock::nowUs();
    uint32_t gapUs = 0;
    if (lastDataEndUs_ > 0 && now >= lastDataEndUs_) {
        gapUs = (uint32_t)(now - lastDataEndUs_);
    }
    emitBotRead10Synth(now, lba, nBytes, durUs, gapUs, tag, kind);
    lastDataEndUs_ = now + (int64_t)durUs;
}

void ReadInstrument::emitBotRead10Synth(int64_t tUs, uint32_t lba, uint32_t nBytes,
                                        uint32_t durUs, uint32_t gapUs, const char* tag,
                                        const char* kind) {
    readCount_++;
    windowBytes_ += nBytes;
    if (!session_ || !session_->active()) return;

    uint16_t blocks = (uint16_t)((nBytes + 511u) / 512u);
    if (blocks == 0) blocks = 1;
    uint32_t botTag = ++botTag_;

    // SCSI READ(10) CDB
    uint8_t cdb[16] = {0};
    cdb[0] = 0x28;
    cdb[2] = (uint8_t)(lba >> 24);
    cdb[3] = (uint8_t)(lba >> 16);
    cdb[4] = (uint8_t)(lba >> 8);
    cdb[5] = (uint8_t)(lba);
    cdb[7] = (uint8_t)(blocks >> 8);
    cdb[8] = (uint8_t)(blocks);

    // BOT CBW (31 bytes), signature USBC
    uint8_t cbw[31] = {0};
    cbw[0] = 'U';
    cbw[1] = 'S';
    cbw[2] = 'B';
    cbw[3] = 'C';
    cbw[4] = (uint8_t)(botTag);
    cbw[5] = (uint8_t)(botTag >> 8);
    cbw[6] = (uint8_t)(botTag >> 16);
    cbw[7] = (uint8_t)(botTag >> 24);
    cbw[8] = (uint8_t)(nBytes);
    cbw[9] = (uint8_t)(nBytes >> 8);
    cbw[10] = (uint8_t)(nBytes >> 16);
    cbw[11] = (uint8_t)(nBytes >> 24);
    cbw[12] = 0x80;  // IN
    cbw[13] = 0;     // LUN
    cbw[14] = 10;    // CBWCBLength
    memcpy(cbw + 15, cdb, 16);

    char cdbHex[33];
    char cbwHex[63];
    bytesToHex(cdb, 16, cdbHex, sizeof(cdbHex));
    bytesToHex(cbw, 31, cbwHex, sizeof(cbwHex));

    {
        JsonDocument doc;
        stampAt(doc, tUs);
        doc["ev"] = "bot_cbw";
        doc["bot_tag"] = botTag;
        doc["bot_source"] = "msc_synth";
        doc["synthesized"] = true;
        doc["raw_hex"] = cbwHex;
        doc["signature"] = "USBC";
        doc["xfer_len"] = nBytes;
        doc["flags"] = 0x80;
        doc["lun"] = 0;
        doc["cdb_len"] = 10;
        doc["cdb_hex"] = cdbHex;
        JsonObject scsi = doc["scsi"].to<JsonObject>();
        scsi["opcode"] = "read10";
        scsi["opcode_byte"] = 0x28;
        scsi["lba"] = lba;
        scsi["blocks"] = blocks;
        scsi["control"] = 0;
        EventBus::instance().publish(doc);
    }

    {
        JsonDocument doc;
        stampAt(doc, tUs);
        doc["ev"] = "bot_data";
        doc["bot_tag"] = botTag;
        doc["bot_source"] = "msc_synth";
        doc["synthesized"] = true;
        doc["dir"] = "in";
        doc["expected"] = nBytes;
        doc["actual"] = nBytes;
        doc["dur_us"] = durUs;
        doc["gap_us"] = gapUs;
        doc["lba"] = lba;
        if (tag) doc["tag"] = tag;
        if (kind) doc["kind"] = kind;
        EventBus::instance().publish(doc);
    }

    // Comfort aggregate before CSW so t_us stays non-decreasing in the chain.
    {
        JsonDocument doc;
        stampAt(doc, tUs);
        doc["ev"] = "read";
        doc["bot_tag"] = botTag;
        doc["bot_source"] = "msc_synth";
        doc["lba"] = lba;
        doc["n"] = nBytes;
        doc["dur_us"] = durUs;
        doc["gap_us"] = gapUs;
        if (tag) doc["tag"] = tag;
        if (kind) doc["kind"] = kind;
        EventBus::instance().publish(doc);
    }

    // CSW (13 bytes), signature USBS
    uint8_t csw[13] = {0};
    csw[0] = 'U';
    csw[1] = 'S';
    csw[2] = 'B';
    csw[3] = 'S';
    csw[4] = (uint8_t)(botTag);
    csw[5] = (uint8_t)(botTag >> 8);
    csw[6] = (uint8_t)(botTag >> 16);
    csw[7] = (uint8_t)(botTag >> 24);
    // residue 0
    csw[12] = 0;  // passed
    char cswHex[27];
    bytesToHex(csw, 13, cswHex, sizeof(cswHex));

    int64_t tCsw = tUs + (int64_t)durUs;
    {
        JsonDocument doc;
        stampAt(doc, tCsw);
        doc["ev"] = "bot_csw";
        doc["bot_tag"] = botTag;
        doc["bot_source"] = "msc_synth";
        doc["synthesized"] = true;
        doc["raw_hex"] = cswHex;
        doc["residue"] = 0;
        doc["status"] = 0;
        EventBus::instance().publish(doc);
    }

    lastDataEndUs_ = tUs + (int64_t)durUs;
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

void ReadInstrument::emitIdentityDescriptors(const char* serial, const char* vendor,
                                             const char* product, const char* revision) {
    if (!session_ || !session_->active()) return;
    int64_t t = ProbeClock::nowUs();
    auto emit = [&](const char* descType, const char* note) {
        JsonDocument doc;
        stampAt(doc, t);
        doc["ev"] = "usb";
        doc["usb_ev"] = "get_descriptor";
        doc["desc_type"] = descType;
        doc["ep"] = 0;
        doc["dir"] = "in";
        doc["dur_us"] = 0;
        doc["bot_source"] = "gadget_identity";
        doc["synthesized"] = true;
        if (note) doc["note"] = note;
        if (serial) doc["serial"] = serial;
        if (vendor) doc["vendor"] = vendor;
        if (product) doc["product"] = product;
        if (revision) doc["revision"] = revision;
        EventBus::instance().publish(doc);
    };
    emit("device", "local gadget identity (not host control sniff)");
    emit("string", serial ? serial : "");
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
