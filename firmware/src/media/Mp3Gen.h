#pragma once

#include <Arduino.h>
#include <cstring>
#include "media/CoverJpegs.h"
#include "media/MediaLayout.h"

/**
 * Random-access MP3 silence generator for SynthFat (0.2.31).
 *
 * File = [ID3v2 (TIT2/TPE1/TALB in fixed 30-byte slots + optional TRCK + optional APIC)]
 *        [optional Xing/Info frame]
 *        [MPEG-1 L3 44.1 kHz joint-stereo frames, zero side info = silence]
 *        [optional ID3v1].
 * Frame lengths follow the bitrate (CBR) or a fixed 4-frame VBR pattern, so any byte
 * offset maps to (frame, byte-in-frame) in O(1). With kStamp each frame carries
 * "HP" + node + seed + frame index in its ancillary bytes (ignored by decoders since
 * part2_3_length = 0) — files are never byte-identical, which unmasks HU caching.
 */
namespace Mp3Gen {

static const uint16_t kKbps[15] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
static constexpr uint32_t kSamplesPerFrame = 1152;
static constexpr uint32_t kSampleRate = 44100;
static constexpr uint8_t kStampOff = 36;  // 4 header + 32 side info (joint stereo)
static constexpr uint8_t kStampLen = 12;
static constexpr uint32_t kId3v1Len = 128;

inline uint8_t kbpsIndex(uint16_t kbps) {
    uint8_t best = 3;
    uint16_t bestDiff = 0xFFFF;
    for (uint8_t i = 1; i < 15; i++) {
        uint16_t d = (kKbps[i] > kbps) ? (kKbps[i] - kbps) : (kbps - kKbps[i]);
        if (d < bestDiff) {
            bestDiff = d;
            best = i;
        }
    }
    return best;
}

inline uint16_t frameLen(uint8_t idx) { return (uint16_t)(144000UL * kKbps[idx] / kSampleRate); }

inline uint8_t clampIdx(int v) { return (uint8_t)(v < 1 ? 1 : (v > 14 ? 14 : v)); }

struct Layout {
    uint32_t id3Fixed = 0;   // ID3v2 bytes before APIC picture data
    uint32_t id3Total = 0;   // whole ID3v2 tag
    uint32_t xingLen = 0;
    uint32_t audioStart = 0;
    uint32_t audioLen = 0;
    uint32_t tailStart = 0;  // ID3v1 offset (== size if none)
    uint8_t pat[4] = {3, 3, 3, 3};
    uint8_t patN = 1;
    uint32_t patBytes = 0;
};

static constexpr uint32_t kTextFrameLen = 10u + 1u + MediaLayout::kTagLen;
static constexpr uint32_t kTrckSlot = 8;  // "255/255" + NUL padding
static constexpr uint32_t kTrckFrameLen = 10u + 1u + kTrckSlot;
static constexpr uint32_t kApicHeadLen = 24;
static constexpr uint32_t kId3FixedMax = 10u + 3u * kTextFrameLen + kTrckFrameLen + kApicHeadLen;

struct Texts {
    char title[MediaLayout::kTagLen + 1];
    char artist[MediaLayout::kTagLen + 1];
    char album[MediaLayout::kTagLen + 1];
};

/** Tag slot text, or the default ("HP <name> v<seed>", "hu-probe", "HUPROBE"). */
inline void resolveTexts(const MediaLayout::Node& n, const MediaLayout::Tags* t, Texts& x) {
    if (t && t->title[0]) snprintf(x.title, sizeof(x.title), "%s", t->title);
    else snprintf(x.title, sizeof(x.title), "HP %s v%u", n.name, (unsigned)n.seed);
    snprintf(x.artist, sizeof(x.artist), "%s", (t && t->artist[0]) ? t->artist : "hu-probe");
    snprintf(x.album, sizeof(x.album), "%s", (t && t->album[0]) ? t->album : "HUPROBE");
}

inline void pattern(const MediaLayout::Node& n, Layout& L) {
    const uint8_t b = kbpsIndex(n.kbps ? n.kbps : 48);
    if (n.flags & MediaLayout::kVbr) {
        L.pat[0] = b;
        L.pat[1] = clampIdx((int)b - 2);
        L.pat[2] = clampIdx((int)b + 1);
        L.pat[3] = clampIdx((int)b - 1);
        L.patN = 4;
    } else {
        L.pat[0] = b;
        L.patN = 1;
    }
    L.patBytes = 0;
    for (uint8_t i = 0; i < L.patN; i++) L.patBytes += frameLen(L.pat[i]);
}

inline uint32_t audioBytesForFrames(const Layout& L, uint32_t frames) {
    uint32_t full = frames / L.patN;
    uint32_t bytes = full * L.patBytes;
    for (uint32_t i = 0; i < frames % L.patN; i++) bytes += frameLen(L.pat[i]);
    return bytes;
}

inline uint32_t framesForAudioBytes(const Layout& L, uint32_t bytes) {
    uint32_t frames = (bytes / L.patBytes) * L.patN;
    uint32_t rem = bytes % L.patBytes;
    for (uint8_t i = 0; i < L.patN; i++) {
        if (rem < frameLen(L.pat[i])) break;
        rem -= frameLen(L.pat[i]);
        frames++;
    }
    return frames;
}

/** Header-side layout (independent of audio length). */
inline void headLayout(const MediaLayout::Node& n, Layout& L) {
    pattern(n, L);
    if (n.flags & MediaLayout::kNoId3v2) {
        L.id3Fixed = L.id3Total = 0;
    } else {
        L.id3Fixed = 10u + 3u * kTextFrameLen + (n.track ? kTrckFrameLen : 0u) +
                     (n.apicBytes ? kApicHeadLen : 0u);
        L.id3Total = L.id3Fixed + n.apicBytes;
    }
    L.xingLen = (n.flags & (MediaLayout::kXing | MediaLayout::kVbr)) ? frameLen(L.pat[0]) : 0;
    L.audioStart = L.id3Total + L.xingLen;
}

inline uint32_t tailLen(const MediaLayout::Node& n) {
    return (n.flags & MediaLayout::kId3v1) ? kId3v1Len : 0;
}

/** Fill n.frames / n.size / n.durMs from either durMs (preferred) or size budget. */
inline void finalizeNode(MediaLayout::Node& n) {
    Layout L;
    headLayout(n, L);
    const uint32_t tail = tailLen(n);
    if (n.durMs > 0) {
        n.frames = (uint32_t)(((uint64_t)n.durMs * kSampleRate + (uint64_t)kSamplesPerFrame * 1000 - 1) /
                              ((uint64_t)kSamplesPerFrame * 1000));
    } else {
        const uint32_t budget = (n.size > L.audioStart + tail) ? (n.size - L.audioStart - tail) : 0;
        n.frames = framesForAudioBytes(L, budget);
    }
    if (n.frames == 0) n.frames = 1;
    n.size = L.audioStart + audioBytesForFrames(L, n.frames) + tail;
    n.durMs = (uint32_t)((uint64_t)n.frames * kSamplesPerFrame * 1000 / kSampleRate);
}

inline void fullLayout(const MediaLayout::Node& n, Layout& L) {
    headLayout(n, L);
    L.audioLen = audioBytesForFrames(L, n.frames);
    L.tailStart = L.audioStart + L.audioLen;
}

inline void be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

inline uint8_t* textFrame(uint8_t* p, const char* id, const char* text) {
    memcpy(p, id, 4);
    be32(p + 4, 1u + MediaLayout::kTagLen);
    p[8] = p[9] = 0;
    p[10] = 0;  // ISO-8859-1, NUL-padded to the slot
    memset(p + 11, 0, MediaLayout::kTagLen);
    memcpy(p + 11, text, strnlen(text, MediaLayout::kTagLen));
    return p + kTextFrameLen;
}

/** ID3v2.3 header + text frames + APIC frame header; b must hold kId3FixedMax bytes. */
inline void buildId3Fixed(const MediaLayout::Node& n, const MediaLayout::Tags* t, const Layout& L,
                          uint8_t* b) {
    const uint32_t body = L.id3Total - 10;
    b[0] = 'I';
    b[1] = 'D';
    b[2] = '3';
    b[3] = 3;
    b[4] = 0;
    b[5] = 0;
    b[6] = (uint8_t)((body >> 21) & 0x7F);
    b[7] = (uint8_t)((body >> 14) & 0x7F);
    b[8] = (uint8_t)((body >> 7) & 0x7F);
    b[9] = (uint8_t)(body & 0x7F);
    Texts x;
    resolveTexts(n, t, x);
    uint8_t* p = textFrame(b + 10, "TIT2", x.title);
    p = textFrame(p, "TPE1", x.artist);
    p = textFrame(p, "TALB", x.album);
    if (n.track) {
        memcpy(p, "TRCK", 4);
        be32(p + 4, 1u + kTrckSlot);
        p[8] = p[9] = 0;
        p[10] = 0;
        memset(p + 11, 0, kTrckSlot);
        snprintf((char*)p + 11, kTrckSlot, "%u", (unsigned)n.track);
        p += kTrckFrameLen;
    }
    if (n.apicBytes) {
        memcpy(p, "APIC", 4);
        be32(p + 4, 1u + 11u + 1u + 1u + n.apicBytes);
        p[8] = p[9] = 0;
        p[10] = 0;
        memcpy(p + 11, "image/jpeg", 11);  // incl. NUL
        p[22] = 0x03;                       // front cover
        p[23] = 0x00;                       // empty description
    }
}

/** Picture byte i of the APIC slot: real JPEG then zero padding, or SOI/EOI placeholder. */
inline uint8_t apicByte(const MediaLayout::Node& n, uint32_t i) {
    if (const CoverJpegs::Cover* c = CoverJpegs::get(n.cover)) {
        return i < c->len ? c->data[i] : 0;
    }
    static const uint8_t kSoi[4] = {0xFF, 0xD8, 0xFF, 0xE0};
    const uint32_t len = n.apicBytes;
    if (i < 4) return kSoi[i];
    if (len >= 6 && i == len - 2) return 0xFF;
    if (len >= 6 && i == len - 1) return 0xD9;
    return 0;
}

static constexpr uint8_t kXingHeadLen = kStampOff + 16;

/** First kXingHeadLen bytes of the Xing/Info frame; the rest of the frame is zero. */
inline void buildXingHead(const MediaLayout::Node& n, const Layout& L, uint8_t* f) {
    memset(f, 0, kXingHeadLen);
    f[0] = 0xFF;
    f[1] = 0xFB;
    f[2] = (uint8_t)(L.pat[0] << 4);
    f[3] = 0x64;
    memcpy(f + kStampOff, (n.flags & MediaLayout::kVbr) ? "Xing" : "Info", 4);
    be32(f + kStampOff + 4, 0x3);  // frames + bytes
    be32(f + kStampOff + 8, n.frames);
    be32(f + kStampOff + 12, n.size);
}

inline void buildId3v1(const MediaLayout::Node& n, const MediaLayout::Tags* tags, uint8_t* t) {
    memset(t, 0, kId3v1Len);
    Texts x;
    resolveTexts(n, tags, x);
    memcpy(t, "TAG", 3);
    memcpy(t + 3, x.title, strnlen(x.title, 30));
    memcpy(t + 33, x.artist, strnlen(x.artist, 30));
    memcpy(t + 63, x.album, strnlen(x.album, 30));
    memcpy(t + 93, "2026", 4);
    if (n.track) {
        // ID3v1.1: 28-byte comment, NUL, track byte.
        snprintf((char*)t + 97, 28, "seed %u", (unsigned)n.seed);
        t[125] = 0;
        t[126] = n.track;
    } else {
        snprintf((char*)t + 97, 30, "seed %u", (unsigned)n.seed);
    }
    t[127] = 12;  // Other
}

/** Write audio frames for audio-relative range [a, a+len) into out (pre-zeroed). */
inline void fillAudio(const MediaLayout::Node& n, uint8_t nodeIdx, const Layout& L, uint32_t a,
                      uint8_t* out, uint32_t len) {
    const bool stamp = (n.flags & MediaLayout::kStamp) != 0;
    uint32_t q = a / L.patBytes;
    uint32_t r = a % L.patBytes;
    uint8_t k = 0;
    while (r >= frameLen(L.pat[k])) {
        r -= frameLen(L.pat[k]);
        k++;
    }
    uint32_t frameIdx = q * L.patN + k;
    uint32_t o = 0;
    while (o < len) {
        const uint16_t fl = frameLen(L.pat[k]);
        const uint8_t hdr[4] = {0xFF, 0xFB, (uint8_t)(L.pat[k] << 4), 0x64};
        uint8_t st[kStampLen] = {'H', 'P', nodeIdx, n.seed, (uint8_t)(frameIdx >> 24),
                                 (uint8_t)(frameIdx >> 16), (uint8_t)(frameIdx >> 8),
                                 (uint8_t)frameIdx, 'F', 'R', 0, 0};
        const uint32_t span = (fl - r < len - o) ? (fl - r) : (len - o);
        const uint32_t lim = stamp ? (uint32_t)(kStampOff + kStampLen) : 4u;
        for (uint32_t j = r; j < lim && j < r + span; j++) {
            uint8_t v = 0;
            if (j < 4) v = hdr[j];
            else if (j >= kStampOff) v = st[j - kStampOff];
            out[o + (j - r)] = v;
        }
        o += span;
        r = 0;
        k = (uint8_t)((k + 1) % L.patN);
        frameIdx++;
    }
}

/** Fill out[0..len) with file bytes at offset off (zero past EOF). */
inline void fill(const MediaLayout::Node& n, const MediaLayout::Tags* tags, uint8_t nodeIdx,
                 uint32_t off, uint8_t* out, uint32_t len) {
    memset(out, 0, len);
    if (off >= n.size) return;
    Layout L;
    fullLayout(n, L);
    const uint32_t end = (off + len < n.size) ? off + len : n.size;
    uint32_t pos = off;
    while (pos < end) {
        uint8_t* dst = out + (pos - off);
        if (pos < L.id3Fixed) {
            uint8_t b[kId3FixedMax];
            buildId3Fixed(n, tags, L, b);
            uint32_t take = L.id3Fixed - pos;
            if (take > end - pos) take = end - pos;
            memcpy(dst, b + pos, take);
            pos += take;
        } else if (pos < L.id3Total) {
            const uint32_t i0 = pos - L.id3Fixed;
            uint32_t take = L.id3Total - pos;
            if (take > end - pos) take = end - pos;
            for (uint32_t i = 0; i < take; i++) dst[i] = apicByte(n, i0 + i);
            pos += take;
        } else if (pos < L.audioStart) {
            uint8_t f[kXingHeadLen];
            buildXingHead(n, L, f);
            const uint32_t i0 = pos - L.id3Total;
            uint32_t take = L.audioStart - pos;
            if (take > end - pos) take = end - pos;
            for (uint32_t j = i0; j < kXingHeadLen && j < i0 + take; j++) dst[j - i0] = f[j];
            pos += take;
        } else if (pos < L.tailStart) {
            uint32_t take = L.tailStart - pos;
            if (take > end - pos) take = end - pos;
            fillAudio(n, nodeIdx, L, pos - L.audioStart, dst, take);
            pos += take;
        } else {
            uint8_t t[kId3v1Len];
            buildId3v1(n, tags, t);
            const uint32_t i0 = pos - L.tailStart;
            uint32_t take = end - pos;
            if (take > kId3v1Len - i0) take = kId3v1Len - i0;
            memcpy(dst, t + i0, take);
            pos += take;
        }
    }
}

}  // namespace Mp3Gen
