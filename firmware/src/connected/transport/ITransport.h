#pragma once

/**
 * @file ITransport.h
 * @brief Agnostic byte-pipe interface under BCL (C1b-0).
 *
 * Transport implementations MUST NOT parse BCL/Etch/SAS. They only move bytes and
 * report connection state. BclFramer lives above this layer.
 */

#include <cstddef>
#include <cstdint>

namespace huprobe {
namespace transport {

/** @brief Coarse transport lifecycle / traffic events (C1b-0). */
enum class TransportEvent : uint8_t {
    None = 0,
    Disconnected = 1,
    Connected = 2,
    RxBytes = 3,
    TxBytes = 4,
    TransportError = 5,
};

inline const char* transportEventName(TransportEvent e) {
    switch (e) {
        case TransportEvent::None: return "NONE";
        case TransportEvent::Disconnected: return "DISCONNECTED";
        case TransportEvent::Connected: return "CONNECTED";
        case TransportEvent::RxBytes: return "RX_BYTES";
        case TransportEvent::TxBytes: return "TX_BYTES";
        case TransportEvent::TransportError: return "TRANSPORT_ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Byte-pipe transport (AOA / SPP / lab inject). No protocol semantics.
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    /** @brief Short id: "aoa" | "spp" | "lab". */
    virtual const char* id() const = 0;

    /** @brief Arm transport (may soft-claim USB). Does not imply Connected. */
    virtual bool arm() = 0;

    /** @brief Disarm / release resources. */
    virtual void disarm() = 0;

    virtual bool armed() const = 0;

    /** @brief True while a peer link is up (AOA bulk configured, SPP open, …). */
    virtual bool connected() const = 0;

    /**
     * @brief Poll hardware / queues; may enqueue RX and raise lastEvent().
     * @note Call from Arduino loop only.
     */
    virtual void poll() = 0;

    /**
     * @brief Copy up to maxLen RX bytes into dst (non-blocking).
     * @return Bytes copied.
     */
    virtual size_t read(uint8_t* dst, size_t maxLen) = 0;

    /**
     * @brief Queue TX bytes (may fail if not connected / skeleton).
     * @return Bytes accepted.
     */
    virtual size_t write(const uint8_t* src, size_t len) = 0;

    virtual uint32_t rxBytes() const = 0;
    virtual uint32_t txBytes() const = 0;

    /** @brief Last event since previous clearLastEvent() / poll notification. */
    virtual TransportEvent lastEvent() const = 0;
    virtual void clearLastEvent() = 0;

    virtual const char* lastError() const = 0;

    /**
     * @brief Implementation maturity for status JSON.
     * skeleton = no real USB/BT path yet; live = hardware bound.
     */
    virtual const char* implLevel() const = 0;
};

}  // namespace transport
}  // namespace huprobe
