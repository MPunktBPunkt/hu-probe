/**
 * @file test_bcl_framer.cpp
 * @brief Host unit tests for C1a BclFramer (synthetic streams only).
 *
 * Evidence markers for all vectors:
 *   source = synthetic
 *   evidence_level = TEST
 *   direct_observation = false
 *
 * Proves parser mechanics — NOT NBT Evo / AOA / BT / Etch / SAS wire behaviour.
 */

#include "connected/bcl/BclFramer.h"
#include "connected/bcl/BclTypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace huprobe::bcl;

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(cond, msg)                                                         \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
            ++g_failed;                                                          \
        } else {                                                                 \
            ++g_passed;                                                          \
        }                                                                        \
    } while (0)

static std::vector<uint8_t> synthFrame(uint16_t cmd, uint16_t src, uint16_t dest,
                                       const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    const bool ok = buildSyntheticFrame(cmd, src, dest,
                                        payload.empty() ? nullptr : payload.data(),
                                        payload.size(), out);
    if (!ok) {
        std::fprintf(stderr, "buildSyntheticFrame failed\n");
        std::exit(2);
    }
    return out;
}

static std::vector<uint8_t> concat(const std::vector<std::vector<uint8_t>>& parts) {
    std::vector<uint8_t> out;
    for (const auto& p : parts) {
        out.insert(out.end(), p.begin(), p.end());
    }
    return out;
}

static void expectFrame(const BclFrame& f, uint16_t cmd, uint16_t src, uint16_t dest,
                        const std::vector<uint8_t>& payload, const char* tag) {
    CHECK(f.cmd == cmd, tag);
    CHECK(f.src == src, tag);
    CHECK(f.dest == dest, tag);
    CHECK(f.len == payload.size(), tag);
    CHECK(f.payload == payload, tag);
    CHECK(f.raw.size() == kHeaderSize + payload.size(), tag);
    CHECK(f.raw.size() >= kHeaderSize, tag);
    if (f.raw.size() >= kHeaderSize) {
        CHECK(f.raw[0] == static_cast<uint8_t>((cmd >> 8) & 0xff), tag);
        CHECK(f.raw[1] == static_cast<uint8_t>(cmd & 0xff), tag);
    }
}

/** T1 — complete frame */
static void test_t1_complete() {
    const std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    const auto bytes = synthFrame(2, 0x0010, 0x0020, payload);
    BclFramer fr;
    auto r = fr.feed(bytes);
    CHECK(r.frames.size() == 1, "T1 frame count");
    CHECK(!r.error, "T1 no error");
    CHECK(!r.incomplete, "T1 not incomplete");
    if (r.frames.size() == 1) {
        expectFrame(r.frames[0], 2, 0x0010, 0x0020, payload, "T1 fields");
    }
}

/** T2 — fragmented frame */
static void test_t2_fragmented() {
    const std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC, 0xDD};
    const auto bytes = synthFrame(4, 1, 2, payload);

    BclFramer one_shot;
    auto full = one_shot.feed(bytes);

    BclFramer frag;
    std::vector<BclFrame> got;
    // chunk sizes: 1, 3, 2, rest
    size_t cuts[] = {1, 3, 2};
    size_t off = 0;
    for (size_t c : cuts) {
        if (off >= bytes.size()) break;
        if (off + c > bytes.size()) c = bytes.size() - off;
        auto r = frag.feed(bytes.data() + off, c);
        CHECK(!r.error, "T2 no error mid");
        got.insert(got.end(), r.frames.begin(), r.frames.end());
        off += c;
    }
    if (off < bytes.size()) {
        auto r = frag.feed(bytes.data() + off, bytes.size() - off);
        CHECK(!r.error, "T2 no error tail");
        got.insert(got.end(), r.frames.begin(), r.frames.end());
    }

    CHECK(got.size() == 1, "T2 one frame");
    CHECK(full.frames.size() == 1, "T2 full one frame");
    if (got.size() == 1 && full.frames.size() == 1) {
        CHECK(got[0] == full.frames[0], "T2 frag == full");
    }
}

/** T3 — multiple frames */
static void test_t3_multiple() {
    auto a = synthFrame(1, 1, 2, {0x11});
    auto b = synthFrame(2, 3, 4, {0x22, 0x33});
    auto c = synthFrame(3, 5, 6, {});
    const auto stream = concat({a, b, c});

    BclFramer fr;
    auto r = fr.feed(stream);
    CHECK(r.frames.size() == 3, "T3 N=3");
    CHECK(!r.error, "T3 no error");
    if (r.frames.size() == 3) {
        expectFrame(r.frames[0], 1, 1, 2, {0x11}, "T3a");
        expectFrame(r.frames[1], 2, 3, 4, {0x22, 0x33}, "T3b");
        expectFrame(r.frames[2], 3, 5, 6, {}, "T3c");
    }
}

/** T4 — zero payload */
static void test_t4_zero_payload() {
    const auto bytes = synthFrame(1, 0x100, 0x200, {});
    BclFramer fr;
    auto r = fr.feed(bytes);
    CHECK(r.frames.size() == 1, "T4 count");
    if (r.frames.size() == 1) {
        CHECK(r.frames[0].len == 0, "T4 len0");
        CHECK(r.frames[0].payload.empty(), "T4 empty payload");
        CHECK(r.frames[0].raw.size() == 8, "T4 raw=8");
    }
}

/** T5 — truncated header */
static void test_t5_truncated_header() {
    const uint8_t partial[] = {0x00, 0x02, 0x00, 0x01, 0x0F};  // 5 < 8
    BclFramer fr;
    auto r = fr.feed(partial, sizeof(partial));
    CHECK(r.frames.empty(), "T5 no frame");
    CHECK(!r.error, "T5 no error");
    CHECK(r.incomplete, "T5 incomplete");
    CHECK(fr.buffered() == 5, "T5 buffered 5");
}

/** T6 — truncated payload */
static void test_t6_truncated_payload() {
    // Header claims len=4, only 2 payload bytes follow
    std::vector<uint8_t> bytes = {
        0x00, 0x02,  // cmd DATA
        0x00, 0x01,  // src
        0x00, 0x02,  // dest
        0x00, 0x04,  // len 4
        0xDE, 0xAD   // only 2 of 4
    };
    BclFramer fr;
    auto r1 = fr.feed(bytes);
    CHECK(r1.frames.empty(), "T6 no frame yet");
    CHECK(!r1.error, "T6 no error");
    CHECK(r1.incomplete, "T6 incomplete");

    const uint8_t rest[] = {0xBE, 0xEF};
    auto r2 = fr.feed(rest, sizeof(rest));
    CHECK(r2.frames.size() == 1, "T6 frame after rest");
    CHECK(!r2.error, "T6 no error after");
    if (r2.frames.size() == 1) {
        expectFrame(r2.frames[0], 2, 1, 2, {0xDE, 0xAD, 0xBE, 0xEF}, "T6 complete");
    }
}

/** T7 — malformed length (> kMaxPayload) */
static void test_t7_malformed_length() {
    // len = 4001 > 4000
    std::vector<uint8_t> hdr = {
        0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        0x0F, 0xA1  // 4001
    };
    BclFramer fr;
    auto r = fr.feed(hdr);
    CHECK(r.frames.empty(), "T7 no phantom");
    CHECK(r.error, "T7 error");
    CHECK(r.error_code == BclParseError::PayloadTooLarge, "T7 code");
    CHECK(fr.hasError(), "T7 sticky error");

    // Further bytes must not emit frames without reset
    const uint8_t junk[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    auto r2 = fr.feed(junk, sizeof(junk));
    CHECK(r2.frames.empty(), "T7 no frame after error");
    CHECK(r2.error, "T7 still error");

    fr.reset();
    auto ok = synthFrame(2, 0, 0, {0x01});
    auto r3 = fr.feed(ok);
    CHECK(r3.frames.size() == 1, "T7 reset recovers");
    CHECK(!r3.error, "T7 reset clear");
}

/** T8 — unknown command value */
static void test_t8_unknown_cmd() {
    const auto bytes = synthFrame(0x00FE, 1, 2, {0x99});
    BclFramer fr;
    auto r = fr.feed(bytes);
    CHECK(r.frames.size() == 1, "T8 parsed");
    if (r.frames.size() == 1) {
        CHECK(r.frames[0].cmd == 0x00FE, "T8 cmd numeric");
        CHECK(std::strcmp(bclCmdDeskName(r.frames[0].cmd), "UNKNOWN") == 0, "T8 name UNKNOWN");
    }
}

/** T9 — dest 0x0FA4 numeric + optional desk label */
static void test_t9_dest_etch_desk() {
    const auto bytes = synthFrame(2, 0x10, kDestEtchDesk, {0x01});
    BclFramer fr;
    auto r = fr.feed(bytes);
    CHECK(r.frames.size() == 1, "T9 frame");
    if (r.frames.size() == 1) {
        CHECK(r.frames[0].dest == 0x0FA4, "T9 dest numeric");
        const char* label = bclDestDeskLabel(r.frames[0].dest);
        CHECK(label != nullptr && std::strcmp(label, "Etch") == 0, "T9 desk label");
        // Parser must not depend on label — feeding same bytes without calling label still works
        CHECK(r.frames[0].cmd == 2, "T9 cmd independent of label");
    }
}

/**
 * T10 — no DEADBEEF magic resync.
 * After hard error, garbage + valid frame must NOT auto-recover without reset().
 * Resync strategy: UNKNOWN / unsupported (documented).
 */
static void test_t10_no_deadbeef_resync() {
    std::vector<uint8_t> bad_hdr = {
        0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF  // 65535 > kMaxPayload
    };
    // Intentionally include DEADBEEF bytes in the following garbage — must NOT be a sync marker.
    std::vector<uint8_t> garbage = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF};
    auto valid = synthFrame(2, 1, 2, {0x42});

    BclFramer fr;
    auto r1 = fr.feed(bad_hdr);
    CHECK(r1.error, "T10 error on oversized");
    auto r2 = fr.feed(garbage);
    CHECK(r2.frames.empty(), "T10 no sync on DEADBEEF");
    CHECK(r2.error, "T10 stays error");
    auto r3 = fr.feed(valid);
    CHECK(r3.frames.empty(), "T10 no phantom after DEADBEEF+valid");
    CHECK(r3.error, "T10 still error until reset");
}

/** Determinism: same input → same frames twice */
static void test_determinism() {
    auto stream = concat({
        synthFrame(4, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}),
        synthFrame(2, 1, kDestEtchDesk, {0x53, 0x59, 0x4E}),  // "SYN" synthetic marker
    });
    BclFramer a, b;
    auto ra = a.feed(stream);
    auto rb = b.feed(stream);
    CHECK(ra.frames.size() == rb.frames.size(), "det count");
    CHECK(ra.frames.size() == 2, "det 2 frames");
    if (ra.frames.size() == rb.frames.size()) {
        for (size_t i = 0; i < ra.frames.size(); ++i) {
            CHECK(ra.frames[i] == rb.frames[i], "det equal");
        }
    }
}

/** Fragmentation invariance over several splits */
static void test_fragmentation_invariance() {
    auto stream = concat({
        synthFrame(1, 9, 8, {1, 2, 3, 4, 5}),
        synthFrame(2, 7, 6, {}),
        synthFrame(12, 0, 0, {0xFF}),
    });

    BclFramer ref;
    auto expected = ref.feed(stream).frames;

    const size_t splits[][8] = {
        {1, 1, 1, 1, 1, 1, 1, 1},
        {8, 5, 8, 0, 1},
        {3, 10, 2, 1000},
        {stream.size()},  // single chunk
    };

    for (size_t si = 0; si < 4; ++si) {
        BclFramer fr;
        std::vector<BclFrame> got;
        size_t off = 0;
        for (size_t j = 0; j < 8 && splits[si][j] != 0; ++j) {
            size_t c = splits[si][j];
            if (off >= stream.size()) break;
            if (off + c > stream.size()) c = stream.size() - off;
            auto r = fr.feed(stream.data() + off, c);
            CHECK(!r.error, "frag inv no err");
            got.insert(got.end(), r.frames.begin(), r.frames.end());
            off += c;
        }
        if (off < stream.size()) {
            auto r = fr.feed(stream.data() + off, stream.size() - off);
            got.insert(got.end(), r.frames.begin(), r.frames.end());
        }
        CHECK(got.size() == expected.size(), "frag inv size");
        if (got.size() == expected.size()) {
            for (size_t i = 0; i < got.size(); ++i) {
                CHECK(got[i] == expected[i], "frag inv frame");
            }
        }
    }
}

/** Explicit: kMaxPayload is 4000 from desk docs, boundary accepted */
static void test_max_payload_boundary() {
    std::vector<uint8_t> payload(kMaxPayload, 0x5A);
    auto bytes = synthFrame(2, 0, 0, payload);
    BclFramer fr;
    auto r = fr.feed(bytes);
    CHECK(r.frames.size() == 1, "max boundary ok");
    CHECK(!r.error, "max boundary no err");
    if (r.frames.size() == 1) {
        CHECK(r.frames[0].len == kMaxPayload, "max len");
    }
}

int main() {
    std::printf("bcl_c1a host tests — source=synthetic evidence_level=TEST "
                "direct_observation=false\n");
    test_t1_complete();
    test_t2_fragmented();
    test_t3_multiple();
    test_t4_zero_payload();
    test_t5_truncated_header();
    test_t6_truncated_payload();
    test_t7_malformed_length();
    test_t8_unknown_cmd();
    test_t9_dest_etch_desk();
    test_t10_no_deadbeef_resync();
    test_determinism();
    test_fragmentation_invariance();
    test_max_payload_boundary();

    std::printf("passed=%d failed=%d\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
