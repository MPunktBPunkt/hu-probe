#pragma once

/**
 * @file SppTransport.h
 * @brief Placeholder for future BT-SPP transport (C1c) — not implemented in C1b-0.
 *
 * BclCarLink uses BT RFCOMM (desk FACT). Same ITransport contract as AoaTransport;
 * no code path armed yet.
 */

#include "connected/transport/ITransport.h"

namespace huprobe {
namespace transport {

/** @brief Stub: all operations fail with not_implemented. */
class SppTransport : public ITransport {
public:
    const char* id() const override { return "spp"; }
    const char* implLevel() const override { return "stub"; }
    bool arm() override { last_error_ = "not_implemented"; return false; }
    void disarm() override {}
    bool armed() const override { return false; }
    bool connected() const override { return false; }
    void poll() override {}
    size_t read(uint8_t*, size_t) override { return 0; }
    size_t write(const uint8_t*, size_t) override { return 0; }
    uint32_t rxBytes() const override { return 0; }
    uint32_t txBytes() const override { return 0; }
    TransportEvent lastEvent() const override { return TransportEvent::TransportError; }
    void clearLastEvent() override {}
    const char* lastError() const override { return last_error_; }

private:
    const char* last_error_ = "not_implemented";
};

}  // namespace transport
}  // namespace huprobe
