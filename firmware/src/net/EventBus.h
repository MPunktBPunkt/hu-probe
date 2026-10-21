#pragma once

/**
 * @file EventBus.h
 * @brief Thread-safe ring buffer of serialized JSONL events awaiting TCP transmission.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "BuildFlags.h"

/**
 * @brief Ring buffer for JSONL events (collector + local trace). Thread-safe.
 *
 * Holds up to EVENT_RING_SIZE serialized lines; when full, publish() overwrites the oldest
 * line and counts it as dropped. Ring access is guarded by a portMUX critical section; the
 * TX/disconnect counters are plain increments.
 */
class EventBus {
public:
    /**
     * @brief Return the process-wide EventBus instance (function-local static).
     * @return Reference to the singleton.
     */
    static EventBus& instance();

    /** @brief Allocate the ring (if needed), clear it and reset all counters; logs on allocation failure. */
    void begin();

    /** @brief Drop all pending lines (frees String heap) — use before Offline RAM alloc. */
    void clear();

    /**
     * @brief Serialize a JSON event and append it as one line to the ring.
     *
     * If the ring is full the oldest line is discarded and dropped() increases. Silently does
     * nothing if the ring cannot be allocated.
     *
     * @param doc Event document to serialize (not modified).
     */
    void publish(JsonDocument& doc);

    /**
     * @brief Copy head without removing (for TCP flush-before-pop).
     * @param[out] out Receives the oldest pending line.
     * @return true if a line was available; false if the ring is empty or unallocated.
     */
    bool peek(String& out);

    /**
     * @brief Remove head after successful TX (or intentional discard).
     * @param[out] out Receives the removed line.
     * @return true if a line was removed; false if the ring is empty or unallocated.
     */
    bool pop(String& out);

    /**
     * @brief Return the number of lines currently queued.
     * @return Pending line count, 0..ringSize(). Read without lock.
     */
    size_t pending() const;

    /** @brief Number of oldest lines overwritten because the ring was full since begin(). */
    uint32_t dropped() const { return dropped_; }

    /** @brief Maximum number of simultaneously queued lines since begin(). */
    uint32_t highwater() const { return highwater_; }

    /** @brief Cumulative number of TCP disconnects reported via noteTcpDisconnect(). */
    uint32_t tcpDisconnects() const { return tcpDisconnects_; }

    /** @brief Count one collector TCP disconnect. */
    void noteTcpDisconnect() { tcpDisconnects_++; }

    /** @brief Hook for a collector TCP connect; intentionally a no-op (only cumulative disconnects are tracked). */
    void noteTcpConnect() { /* cumulative disconnects only */ }

    /** @brief Total number of lines published since begin(). */
    uint32_t published() const { return published_; }

    /** @brief Number of events reported lost outside the ring via noteStreamLost(). */
    uint32_t streamLost() const { return streamLost_; }

    /**
     * @brief Add to the count of events lost outside the ring.
     * @param n Number of lost events (default 1).
     */
    void noteStreamLost(uint32_t n = 1) { streamLost_ += n; }

    /** @brief Number of lines successfully written to the collector. */
    uint32_t tcpTxOk() const { return tcpTxOk_; }

    /** @brief Number of failed (short) collector writes. */
    uint32_t tcpTxFail() const { return tcpTxFail_; }

    /** @brief Count one successful line TX and record its millis() timestamp for lastTcpTxOkMs(). */
    void noteTcpTxOk();

    /** @brief Count one failed (short) collector write. */
    void noteTcpTxFail() { tcpTxFail_++; }

    /** @brief millis() of last successful TCP event TX (0 = never). */
    unsigned long lastTcpTxOkMs() const { return lastTcpTxOkMs_; }

    /** @brief True if the ring slot array was allocated in PSRAM. */
    bool ringInPsram() const { return ringInPsram_; }

    /** @brief Ring capacity in lines (EVENT_RING_SIZE). */
    size_t ringSize() const { return ringSize_; }

    /**
     * @brief Write the queue/TX statistics into a capture_health JSON object.
     * @param[out] obj        Target JSON object (event_seq_*, dropped, published, tcp_*, queue_*, ring_*).
     * @param seqFirst        First event sequence number of the session.
     * @param seqLast         Last event sequence number of the session.
     * @param tcpConnected    Current collector socket state.
     */
    void fillCaptureHealth(JsonObject obj, uint32_t seqFirst, uint32_t seqLast,
                           bool tcpConnected) const;

private:
    EventBus() = default;

    /**
     * @brief Lazily allocate the ring slot array, preferring PSRAM, and construct empty Strings.
     *
     * Slot objects in PSRAM (~32–64 KiB for 2048 Strings) — payloads still use heap, but DRAM
     * pressure from the slot array itself drops sharply.
     *
     * @return true if the ring exists or was allocated; false on allocation failure.
     */
    bool ensureRing();

    String* ring_ = nullptr;
    size_t ringSize_ = EVENT_RING_SIZE;
    bool ringInPsram_ = false;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    uint32_t dropped_ = 0;
    uint32_t highwater_ = 0;
    uint32_t tcpDisconnects_ = 0;
    uint32_t published_ = 0;
    uint32_t streamLost_ = 0;
    uint32_t tcpTxOk_ = 0;
    uint32_t tcpTxFail_ = 0;
    unsigned long lastTcpTxOkMs_ = 0;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};
