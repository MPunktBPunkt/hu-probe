#pragma once

/**
 * @file BclFramer.h
 * @brief Streaming BCL 8-byte-header framer (host-testable, no transport/SAS/Etch).
 *
 * C1a: deterministic parse of synthetic streams only.
 * No DEADBEEF / magic resync — after PayloadTooLarge the framer stays in error until reset().
 */

#include "connected/bcl/BclTypes.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace huprobe {
namespace bcl {

/**
 * @brief Result of one feed() call: zero or more complete frames, optional hard error.
 *
 * incomplete=true means bytes are buffered waiting for more input (not an error).
 * After error, further feed() calls add no frames until reset().
 */
struct BclFeedResult {
    std::vector<BclFrame> frames;
    bool incomplete = false;
    bool error = false;
    BclParseError error_code = BclParseError::None;
    /** Stable ASCII reason for JSONL / tests; empty if no error. */
    const char* error_detail = "";
};

/**
 * @brief Deterministic streaming BCL framer.
 *
 * Fragmentation-invariant: parse(S) equals parse(chunk1)+…+parse(chunkN) for any split of S
 * (until a hard error; after error, reset is required — no invented resync).
 */
class BclFramer {
public:
    BclFramer() { reset(); }

    /** @brief Clear buffer and error; ready for a new stream. */
    void reset();

    /**
     * @brief Append bytes and emit every complete frame that becomes available.
     * @param data  Input bytes (may be nullptr if n==0).
     * @param n     Byte count.
     * @return Frames emitted this call; incomplete/error flags for buffer state.
     */
    BclFeedResult feed(const uint8_t* data, size_t n);

    /** @brief Convenience overload for a contiguous buffer. */
    BclFeedResult feed(const std::vector<uint8_t>& data) {
        return feed(data.empty() ? nullptr : data.data(), data.size());
    }

    /** @brief True after PayloadTooLarge until reset(). */
    bool hasError() const { return error_; }

    BclParseError lastError() const { return error_code_; }

    /** @brief Bytes currently held waiting for a complete frame (0 if idle/error). */
    size_t buffered() const { return fill_; }

    /** @brief True if buffered bytes form a partial header or partial payload. */
    bool isIncomplete() const { return !error_ && fill_ > 0; }

private:
    void tryEmit(BclFeedResult& out);

    uint8_t buf_[kHeaderSize + kMaxPayload];
    size_t fill_ = 0;
    bool error_ = false;
    BclParseError error_code_ = BclParseError::None;
};

}  // namespace bcl
}  // namespace huprobe
