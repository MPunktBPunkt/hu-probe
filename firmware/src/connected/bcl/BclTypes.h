#pragma once

/**
 * @file BclTypes.h
 * @brief BCL frame types for the Connected-App control-plane probe (C1a).
 *
 * Header layout from BimmerGestalt desk evidence (R0), not own wire captures:
 *   command u16 BE | src u16 BE | dest u16 BE | dataLen u16 BE | payload
 * Sources: EVID-BG-2026-10-28-001, bmw-bcl SHEET, BMWConnectedAnalysis protocol-bcl.md
 *          ("up to 4000 bytes"), BclCarLink BclPacket.kt serialize/readFrom.
 *
 * Epistemic: synthetic host tests prove parser mechanics only — not NBT Evo wire FACT.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace huprobe {
namespace bcl {

/** @brief BCL header size in bytes (4 × u16 BE). */
constexpr size_t kHeaderSize = 8;

/**
 * @brief Practical max payload from desk docs (protocol-bcl.md: "up to 4000 bytes";
 *        BclCarLink TcpMuxer inputBuffer 4000). Not an OEM-spec claim.
 */
constexpr size_t kMaxPayload = 4000;

/** @brief Desk-evidence dest for Etch RPC (EVID-BG-2026-10-28-002). Numeric only in core. */
constexpr uint16_t kDestEtchDesk = 0x0FA4;

/** @brief Desk-evidence dest for Watchdog (EVID-BG-2026-10-28-007). */
constexpr uint16_t kDestWatchdogDesk = 0x1389;

/**
 * @brief Desk command values from BclCarLink COMMAND enum / wireshark_bmw_bcl
 *        (EVID-BG-2026-10-28-001). Unknown wire values stay numeric — no invented semantics.
 */
enum class BclCmd : uint16_t {
    Unknown = 0,
    Open = 1,
    Data = 2,
    Close = 3,
    Handshake = 4,
    SelectProto = 5,
    DataAck = 6,
    Ack = 7,
    Knock = 8,
    Launch = 9,
    Hangup = 10,
    Broadcast = 11,
    Register = 12,
};

/** @brief One complete BCL frame (header fields + payload + raw bytes). */
struct BclFrame {
    uint16_t cmd = 0;
    uint16_t src = 0;
    uint16_t dest = 0;
    uint16_t len = 0;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> raw;

    bool operator==(const BclFrame& o) const {
        return cmd == o.cmd && src == o.src && dest == o.dest && len == o.len &&
               payload == o.payload && raw == o.raw;
    }
};

/** @brief Parser hard-error codes (no automatic resync — see docs). */
enum class BclParseError : uint8_t {
    None = 0,
    /** dataLen exceeds kMaxPayload (desk practical limit). */
    PayloadTooLarge = 1,
};

/**
 * @brief Optional desk command name for display/tests. Returns "UNKNOWN" if not in enum.
 * @note Presentation helper — parser core does not require this.
 */
inline const char* bclCmdDeskName(uint16_t cmd) {
    switch (cmd) {
        case 1: return "OPEN";
        case 2: return "DATA";
        case 3: return "CLOSE";
        case 4: return "HANDSHAKE";
        case 5: return "SELECTPROTO";
        case 6: return "DATAACK";
        case 7: return "ACK";
        case 8: return "KNOCK";
        case 9: return "LAUNCH";
        case 10: return "HANGUP";
        case 11: return "BROADCAST";
        case 12: return "REGISTER";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Optional desk dest label. nullptr if no desk label for this value.
 * @note Presentation only — never used for framing decisions.
 */
inline const char* bclDestDeskLabel(uint16_t dest) {
    if (dest == kDestEtchDesk) return "Etch";
    if (dest == kDestWatchdogDesk) return "Watchdog";
    return nullptr;
}

/**
 * @brief Build a synthetic BCL frame byte stream (test / lab scenario only).
 * @param cmd   Command field (u16).
 * @param src   Source id (u16).
 * @param dest  Dest id (u16).
 * @param payload Payload bytes; size must be <= kMaxPayload.
 * @param[out] out Appends 8 + payload.size() bytes.
 * @return false if payload too large.
 */
inline bool buildSyntheticFrame(uint16_t cmd, uint16_t src, uint16_t dest,
                                const uint8_t* payload, size_t payloadLen,
                                std::vector<uint8_t>& out) {
    if (payloadLen > kMaxPayload) return false;
    out.push_back(static_cast<uint8_t>((cmd >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(cmd & 0xff));
    out.push_back(static_cast<uint8_t>((src >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(src & 0xff));
    out.push_back(static_cast<uint8_t>((dest >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(dest & 0xff));
    const uint16_t len = static_cast<uint16_t>(payloadLen);
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(len & 0xff));
    if (payloadLen && payload) {
        out.insert(out.end(), payload, payload + payloadLen);
    }
    return true;
}

}  // namespace bcl
}  // namespace huprobe
