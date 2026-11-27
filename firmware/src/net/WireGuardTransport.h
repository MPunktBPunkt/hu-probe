#pragma once

/**
 * @file WireGuardTransport.h
 * @brief Optional WireGuard overlay (compile-time gated). MSC/capture never depend on this.
 */

#include <Arduino.h>

class ConfigStore;

/**
 * @brief Thin adapter around vendored esp_wireguard (only when HUPROBE_ENABLE_WIREGUARD=1).
 *
 * enable() is best-effort; failures are logged only. loop() polls peer state periodically.
 */
class WireGuardTransport {
public:
    void begin(ConfigStore* cfg);
    /** @brief Init + connect from ConfigStore (no-op when disabled or unconfigured). */
    bool enable();
    void disable();
    void loop();
    bool active() const;
    bool peerUp() const;
    const char* lastError() const;

private:
    ConfigStore* cfg_ = nullptr;
    bool attempted_ = false;
    bool active_ = false;
    bool peerUp_ = false;
    unsigned long lastPeerPollMs_ = 0;
};