#include "EventBus.h"

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

void EventBus::begin() {
    head_ = tail_ = count_ = 0;
    dropped_ = 0;
}

void EventBus::publish(JsonDocument& doc) {
    String line;
    serializeJson(doc, line);
    if (count_ >= EVENT_RING_SIZE) {
        // drop oldest
        head_ = (head_ + 1) % EVENT_RING_SIZE;
        count_--;
        dropped_++;
    }
    ring_[tail_] = line;
    tail_ = (tail_ + 1) % EVENT_RING_SIZE;
    count_++;
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
