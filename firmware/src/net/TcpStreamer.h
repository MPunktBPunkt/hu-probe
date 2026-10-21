#pragma once

/**
 * @file TcpStreamer.h
 * @brief TCP client that streams EventBus JSONL lines to the collector.
 */

#include <Arduino.h>
#include <WiFiClient.h>
#include "core/ConfigStore.h"

/** @brief TCP client → collector :9100, JSONL lines. */
class TcpStreamer {
public:
    /**
     * @brief Store the configuration and reset the connection state.
     * @param config Configuration store (enableCollector, collectorHost, collectorPort); must outlive the streamer.
     */
    void begin(ConfigStore* config);

    /**
     * @brief Track disconnects, reconnect if needed and flush pending EventBus lines.
     *
     * Does nothing if the collector is disabled or WiFi is not connected.
     *
     * @warning Not thread-safe. Sole owner must be the Arduino loop (and OfflineCapture
     * pumps that run on that same stack during stored replay). Do not call from wifiCoexTask.
     */
    void loop();

    /** @brief True if the collector socket is currently connected. */
    bool connected() const { return const_cast<WiFiClient&>(client_).connected(); }

    /**
     * @brief Offline replay: smaller batches + TCP flush so WiFi TX completes.
     * @param on true for offline drain (8 lines per flush, flush + 2 ms delay per line); false for live streaming (32 lines).
     */
    void setOfflineDrain(bool on) { offlineDrain_ = on; }

    /**
     * @brief Drop a stuck collector socket and allow an immediate reconnect attempt.
     *
     * Used by trip-upload drain when TX stalls with lines still pending. Resets the connect
     * throttle so the next loop()/tryConnect() is not blocked by the 3 s backoff.
     */
    void forceReconnect();

private:
    /** @brief Attempt a collector connect at most every 3 s (blocking) and flush immediately on success. */
    void tryConnect();

    /**
     * @brief Send up to one batch of pending lines (peek → write → pop).
     *
     * Each line is written atomically as line + '\n'. On a short write the socket is closed
     * and the line stays at the head for retry after reconnect, so no seq is lost.
     */
    void flushPending();

    ConfigStore* config_ = nullptr;
    WiFiClient client_;
    unsigned long lastConnectAttempt_ = 0;
    bool wasConnected_ = false;
    bool offlineDrain_ = false;
    /** @brief Scratch for atomic line+"\\n" write (avoids heap String during offline drain). */
    uint8_t txBuf_[1536];
};
