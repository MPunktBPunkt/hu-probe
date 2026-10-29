#pragma once

/**
 * @file CaC1LabBclStream.h
 * @brief Deterministic synthetic byte stream for scenario ca_c1_lab_bcl (C1a / C1a.5).
 *
 * source=synthetic · evidence_level=TEST · direct_observation=false
 * Same bytes on host tools and ESP32 — not NBT/AOA wire evidence.
 */

#include "connected/bcl/BclTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace huprobe {
namespace bcl {

/** @brief Fixed chunk sizes used by the lab scenario (fragmentation exercise). */
inline const size_t kCaC1LabBclChunks[] = {1, 3, 2, 8, 5, 1000};
inline constexpr size_t kCaC1LabBclChunkCount =
    sizeof(kCaC1LabBclChunks) / sizeof(kCaC1LabBclChunks[0]);

/**
 * @brief Build the fixed ca_c1_lab_bcl synthetic stream.
 *
 * 1) HANDSHAKE cmd=4, 8 zero payload
 * 2) DATA dest=0x0FA4, payload "SYNTH01"
 * 3) Unknown cmd 0x00EE, zero payload
 * 4) Malformed len=4001 header → PayloadTooLarge
 */
inline std::vector<uint8_t> buildCaC1LabBclStream() {
    std::vector<uint8_t> stream;
    const uint8_t hs[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    buildSyntheticFrame(4, 0, 0, hs, 8, stream);

    const char* synth = "SYNTH01";
    buildSyntheticFrame(2, 1, kDestEtchDesk, reinterpret_cast<const uint8_t*>(synth), 7, stream);

    buildSyntheticFrame(0x00EE, 0, 0, nullptr, 0, stream);

    const uint8_t bad[] = {0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xA1};
    stream.insert(stream.end(), bad, bad + sizeof(bad));
    return stream;
}

}  // namespace bcl
}  // namespace huprobe
