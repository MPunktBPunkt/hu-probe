#include "EventBus.h"

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

void EventBus::begin() {
    clear();
    portENTER_CRITICAL(&mux_);
    dropped_ = 0;
    highwater_ = 0;
    tcpDisconnects_ = 0;
    published_ = 0;
    streamLost_ = 0;
    tcpTxOk_ = 0;
    tcpTxFail_ = 0;
    lastTcpTxOkMs_ = 0;
    portEXIT_CRITICAL(&mux_);
}

void EventBus::clear() {
    portENTER_CRITICAL(&mux_);
    while (count_ > 0) {
        ring_[head_] = String();
        head_ = (head_ + 1) % EVENT_RING_SIZE;
        count_--;
    }
    head_ = tail_ = count_ = 0;
    portEXIT_CRITICAL(&mux_);
}

void EventBus::noteTcpTxOk() {
    tcpTxOk_++;
    lastTcpTxOkMs_ = millis();
}

void EventBus::publish(JsonDocument& doc) {
    String line;
    serializeJson(doc, line);
    portENTER_CRITICAL(&mux_);
    if (count_ >= EVENT_RING_SIZE) {
        head_ = (head_ + 1) % EVENT_RING_SIZE;
        count_--;
        dropped_++;
    }
    ring_[tail_] = line;
    tail_ = (tail_ + 1) % EVENT_RING_SIZE;
    count_++;
    published_++;
    if (count_ > highwater_) highwater_ = (uint32_t)count_;
    portEXIT_CRITICAL(&mux_);
}

bool EventBus::peek(String& out) {
    portENTER_CRITICAL(&mux_);
    if (count_ == 0) {
        portEXIT_CRITICAL(&mux_);
        return false;
    }
    out = ring_[head_];
    portEXIT_CRITICAL(&mux_);
    return true;
}

bool EventBus::pop(String& out) {
    portENTER_CRITICAL(&mux_);
    if (count_ == 0) {
        portEXIT_CRITICAL(&mux_);
        return false;
    }
    out = ring_[head_];
    head_ = (head_ + 1) % EVENT_RING_SIZE;
    count_--;
    portEXIT_CRITICAL(&mux_);
    return true;
}

size_t EventBus::pending() const {
    return count_;
}

void EventBus::fillCaptureHealth(JsonObject obj, uint32_t seqFirst, uint32_t seqLast,
                                 bool tcpConnected) const {
    obj["event_seq_first"] = seqFirst;
    obj["event_seq_last"] = seqLast;
    obj["dropped"] = dropped_;
    obj["published"] = published_;
    obj["stream_lost"] = streamLost_;
    obj["tcp_tx_ok"] = tcpTxOk_;
    obj["tcp_tx_fail"] = tcpTxFail_;
    obj["queue_pending"] = (uint32_t)count_;
    obj["queue_highwater"] = highwater_;
    obj["tcp_connected"] = tcpConnected;
    obj["tcp_disconnects"] = tcpDisconnects_;
    obj["ring_size"] = EVENT_RING_SIZE;
}
