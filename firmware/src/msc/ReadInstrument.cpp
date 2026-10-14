#include "ReadInstrument.h"
#include "net/EventBus.h"
#include <cstring>

void ReadInstrument::begin(Session* session) {
    session_ = session;
    windowStartMs_ = millis();
}

void ReadInstrument::onRead(uint32_t lba, uint32_t nBytes, uint32_t durUs,
                            const char* tag, const char* kind) {
    readCount_++;
    windowBytes_ += nBytes;
    unsigned long now = millis();
    uint32_t gapUs = 0;
    if (lastReadMs_ > 0) gapUs = (uint32_t)((now - lastReadMs_) * 1000UL);
    lastReadMs_ = now;

    if (!session_ || !session_->active()) return;

    JsonDocument doc;
    doc["v"] = 1;
    doc["sid"] = session_->id();
    doc["seq"] = session_->nextSeq();
    doc["t_ms"] = (int)now;
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
    doc["v"] = 1;
    doc["sid"] = session_->id();
    doc["seq"] = session_->nextSeq();
    doc["t_ms"] = (int)millis();
    doc["ev"] = "scsi";
    doc["op"] = op ? op : "other";
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
