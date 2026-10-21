/**
 * @file EventBus.cpp
 * @brief EventBus ring allocation, publish/peek/pop and capture-health statistics.
 */

#include "EventBus.h"
#include "core/HuMem.h"
#include <utility>

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

bool EventBus::ensureRing() {
    if (ring_) return true;
    // Slot objects in PSRAM (~32–64 KiB for 2048 Strings) — payloads still use heap,
    // but DRAM pressure from the slot array itself drops sharply.
    void* mem = HuMem::callocPreferPsram(EVENT_RING_SIZE, sizeof(String));
    if (!mem) return false;
    ring_ = static_cast<String*>(mem);
    ringSize_ = EVENT_RING_SIZE;
    ringInPsram_ = HuMem::ptrInPsram(ring_);
    for (size_t i = 0; i < ringSize_; i++) {
        new (&ring_[i]) String();
    }
    Serial.printf("[BUS] ring slots=%u in_psram=%d free_heap=%u free_psram=%u\n",
                  (unsigned)ringSize_, ringInPsram_ ? 1 : 0, (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getFreePsram());
    return true;
}

void EventBus::begin() {
    if (!ensureRing()) {
        Serial.println("[BUS] ring alloc FAILED");
        return;
    }
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
    if (!ring_) return;
    portENTER_CRITICAL(&mux_);
    while (count_ > 0) {
        ring_[head_] = String();
        head_ = (head_ + 1) % ringSize_;
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
    if (!ensureRing()) return;
    String line;
    serializeJson(doc, line);
    portENTER_CRITICAL(&mux_);
    if (count_ >= ringSize_) {
        // Free overwritten payload — otherwise DRAM leaks for the whole ring life.
        ring_[head_] = String();
        head_ = (head_ + 1) % ringSize_;
        count_--;
        dropped_++;
    }
    ring_[tail_] = std::move(line);
    tail_ = (tail_ + 1) % ringSize_;
    count_++;
    published_++;
    if (count_ > highwater_) highwater_ = (uint32_t)count_;
    portEXIT_CRITICAL(&mux_);
}

bool EventBus::peek(String& out) {
    portENTER_CRITICAL(&mux_);
    if (!ring_ || count_ == 0) {
        portEXIT_CRITICAL(&mux_);
        return false;
    }
    out = ring_[head_];
    portEXIT_CRITICAL(&mux_);
    return true;
}

bool EventBus::pop(String& out) {
    portENTER_CRITICAL(&mux_);
    if (!ring_ || count_ == 0) {
        portEXIT_CRITICAL(&mux_);
        return false;
    }
    // Move out and clear slot. Leaving the String in the ring after pop leaked ~300 B
    // per event during stored replay (.89 0.3.16: heap 184→21 KiB over ~463 tx_ok).
    out = std::move(ring_[head_]);
    ring_[head_] = String();
    head_ = (head_ + 1) % ringSize_;
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
    obj["ring_size"] = (uint32_t)ringSize_;
    obj["ring_in_psram"] = ringInPsram_;
}
