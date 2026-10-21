#pragma once

/**
 * @file HubClient.h
 * @brief Heartbeat and Hub-OTA client for the esp-hub.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "ConfigStore.h"

/**
 * @brief Heartbeat to /api/register plus Hub-OTA (esp-hub contract).
 *
 * The hub reply may adjust the heartbeat interval and advertise an otaUrl that is then
 * flashed from loop(). Heartbeat and OTA are suppressed while offline capture owns RAM + WiFi.
 */
class HubClient {
public:
    /**
     * @brief Bind the configuration and reset the heartbeat timer (first heartbeat on next loop()).
     * @param config  Configuration (hub host/port, interval, enable flag); must outlive the client.
     */
    void begin(ConfigStore* config);

    /**
     * @brief Send a heartbeat when the interval (min. 5 s) has elapsed and run a pending OTA.
     *
     * No-op if the hub is disabled. While OfflineCapture blocks WiFi supervision any pending
     * OTA is discarded.
     * @warning A pending OTA blocks and ends in ESP.restart() on success.
     */
    void loop();

    /** @brief Send a heartbeat immediately and restart the interval timer. */
    void sendNow();

    /**
     * @brief POST a JSON payload to the hub (http://hubHost:hubPort + path).
     * @param path       URL path starting with '/'.
     * @param payload    Serialized JSON body.
     * @param timeoutMs  HTTP timeout in ms.
     * @return HTTP status code (200 also updates lastSuccessMs()), negative HTTPClient error,
     *         or -1 if not configured, path is null or WLAN is not connected.
     */
    int postJson(const char* path, const String& payload, uint16_t timeoutMs = 20000);

    /** @brief true if the last heartbeat got HTTP 200 (false while WLAN is down). */
    bool lastOk() const { return lastOk_; }

    /** @brief millis() of the last HTTP 200 from the hub (initialized to begin() time). */
    unsigned long lastSuccessMs() const { return lastSuccess_; }

    /**
     * @brief Set the callback that fills the heartbeat JSON document.
     * @param builder  Function filling the register payload; nullptr sends an empty document.
     */
    void setPayloadBuilder(void (*builder)(JsonDocument& doc));

    /** @brief true if the hub advertised an accepted otaUrl that loop() has not flashed yet. */
    bool otaPending() const { return otaPending_; }

private:
    /**
     * @brief POST the register payload (2 s timeout) and evaluate the hub reply.
     *
     * Accepts a new "interval" in 5..600 s and an "otaUrl"; the cross-flash guard ignores
     * PiDrive (non-hu-probe) URLs and any URL while offline capture is active.
     */
    void sendHeartbeat();

    /**
     * @brief Download and flash a firmware image from the hub, then restart.
     * @param url  HTTP URL of the firmware binary.
     * @warning Blocks (30 s HTTP timeout) and does not return on success (ESP.restart()).
     */
    void performOta(const String& url);

    ConfigStore* config_ = nullptr;
    void (*builder_)(JsonDocument& doc) = nullptr;
    unsigned long lastHeartbeat_ = 0;
    unsigned long lastSuccess_ = 0;
    bool lastOk_ = false;
    bool otaPending_ = false;
    String otaUrl_;
};
