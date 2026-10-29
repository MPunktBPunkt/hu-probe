/**
 * @file BclFramer.cpp
 * @brief Streaming BCL framer implementation (C1a).
 */

#include "connected/bcl/BclFramer.h"

#include <cstring>

namespace huprobe {
namespace bcl {

namespace {

uint16_t readBe16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

}  // namespace

void BclFramer::reset() {
    fill_ = 0;
    error_ = false;
    error_code_ = BclParseError::None;
}

void BclFramer::tryEmit(BclFeedResult& out) {
    while (!error_) {
        if (fill_ < kHeaderSize) {
            out.incomplete = (fill_ > 0);
            return;
        }

        const uint16_t cmd = readBe16(buf_ + 0);
        const uint16_t src = readBe16(buf_ + 2);
        const uint16_t dest = readBe16(buf_ + 4);
        const uint16_t len = readBe16(buf_ + 6);

        if (static_cast<size_t>(len) > kMaxPayload) {
            error_ = true;
            error_code_ = BclParseError::PayloadTooLarge;
            out.error = true;
            out.error_code = BclParseError::PayloadTooLarge;
            out.error_detail = "payload_len_exceeds_kMaxPayload";
            out.incomplete = false;
            // Leave fill_ as-is for diagnostics; no phantom frame; no resync scan.
            return;
        }

        const size_t need = kHeaderSize + static_cast<size_t>(len);
        if (fill_ < need) {
            out.incomplete = true;
            return;
        }

        BclFrame frame;
        frame.cmd = cmd;
        frame.src = src;
        frame.dest = dest;
        frame.len = len;
        frame.payload.assign(buf_ + kHeaderSize, buf_ + need);
        frame.raw.assign(buf_, buf_ + need);
        out.frames.push_back(std::move(frame));

        const size_t remain = fill_ - need;
        if (remain > 0) {
            std::memmove(buf_, buf_ + need, remain);
        }
        fill_ = remain;
    }
}

BclFeedResult BclFramer::feed(const uint8_t* data, size_t n) {
    BclFeedResult out;

    if (error_) {
        out.error = true;
        out.error_code = error_code_;
        out.error_detail = "framer_in_error_reset_required";
        return out;
    }

    if (n == 0 || data == nullptr) {
        out.incomplete = (fill_ > 0);
        return out;
    }

    size_t off = 0;
    while (off < n && !error_) {
        const size_t room = sizeof(buf_) - fill_;
        if (room == 0) {
            // Should not happen: max frame fits in buf_; incomplete waits below capacity.
            // If we somehow fill without a complete frame, treat as payload-too-large path
            // already covered by len check. Defensive stop:
            error_ = true;
            error_code_ = BclParseError::PayloadTooLarge;
            out.error = true;
            out.error_code = BclParseError::PayloadTooLarge;
            out.error_detail = "internal_buffer_full";
            return out;
        }
        const size_t take = (n - off) < room ? (n - off) : room;
        std::memcpy(buf_ + fill_, data + off, take);
        fill_ += take;
        off += take;
        tryEmit(out);
    }

    if (!out.error) {
        out.incomplete = (fill_ > 0);
    }
    return out;
}

}  // namespace bcl
}  // namespace huprobe
