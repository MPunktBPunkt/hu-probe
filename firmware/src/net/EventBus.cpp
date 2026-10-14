#include "EventBus.h"

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

void EventBus::begin() {
    head_ = tail_ = count_ = 0;
    dropped_ = 0;
    highwater_ = 0;
    tcpDisconnects_ = 0;
    published_ = 0;
}

void EventBus::publish(JsonDocument& doc) {
    String line;
    serializeJson(doc, line);
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
}

bool EventBus::pop(String& out) {
    if (count_ == 0) return false;
    out = ring_[head_];
    head_ = (head_ + 1) % EVENT_RING_SIZE;
    count_--;
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
    obj["queue_pending"] = (uint32_t)count_;
    obj["queue_highwater"] = highwater_;
    obj["tcp_connected"] = tcpConnected;
    obj["tcp_disconnects"] = tcpDisconnects_;
    obj["ring_size"] = EVENT_RING_SIZE;
}
