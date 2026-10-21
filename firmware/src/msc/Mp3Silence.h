#pragma once

/**
 * @file Mp3Silence.h
 * @brief Header-only generator for CBR MP3 silence (with M3 lab markers) served as MSC file content.
 */

#include <Arduino.h>
#include <cstring>

/**
 * @brief CBR MP3 silence filler for MSC station stubs (Baustelle B2).
 *
 * Tiles a fixed 48 kbit/s MPEG-1 Layer III stereo frame so HU decoders
 * hold the track for the full slot size instead of skipping on 0xFF padding.
 */
namespace Mp3Silence {

/** @brief Frame length in bytes: one MPEG-1 L3 stereo 48 kb/s frame @ 44.1 kHz (pad=0), length 156. */
static constexpr uint16_t kFrameLen = 156;

/** @brief Approximate MP3 byte rate used for marker spacing; M3 lab markers: distinctive stamp every ~30 s @ 48 kbit/s (≈6000 B/s). */
static constexpr uint32_t kBytesPerSec = 6000;

/** @brief Body distance in bytes between two M3 markers (30 s x kBytesPerSec = 180000). */
static constexpr uint32_t kMarkerIntervalBytes = 30u * kBytesPerSec;  // 180000

/** @brief Marker stamp byte 0 ('P' of "PDMK"), written at frame offset 4. */
static constexpr uint8_t kMarkerMagic0 = 'P';

/** @brief Marker stamp byte 1 ('D' of "PDMK"), written at frame offset 5. */
static constexpr uint8_t kMarkerMagic1 = 'D';

/** @brief Marker stamp byte 2 ('M' of "PDMK"), written at frame offset 6. */
static constexpr uint8_t kMarkerMagic2 = 'M';

/** @brief Marker stamp byte 3 ('K' of "PDMK"), written at frame offset 7. */
static constexpr uint8_t kMarkerMagic3 = 'K';

/** @brief Raw bytes of the silent 48 kb/s MPEG-1 L3 frame that is tiled after the head. */
// clang-format off
static const uint8_t kFrame[kFrameLen] = {
    0xff, 0xfb, 0x30, 0x64, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
    0x0d, 0x20, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0xa4, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x34,
    0x80, 0x00, 0x00, 0x04, 0x4c, 0x41, 0x4d, 0x45, 0x33, 0x2e, 0x31, 0x30, 0x30, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x4c, 0x41, 0x4d, 0x45, 0x33, 0x2e, 0x31, 0x30, 0x30, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
};
// clang-format on

/** @brief Head length in bytes written by writeHead(): minimal ID3v2.4 + LAME Info frame (208 B audio header @ 64 kb/s) with patchable size. */
static constexpr uint16_t kHeadLen = 252;  // 44 ID3 + 208 Info frame

/**
 * @brief Store a 32-bit value big-endian at p.
 * @param p  Destination; must have room for 4 bytes.
 * @param v  Value to store (MSB first).
 */
inline void writeBe32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/**
 * @brief Build the ID3 + Info/Xing head for a file of fileSize bytes.
 *
 * Writes kHeadLen bytes: a fixed 44-byte ID3 tag followed by a 208-byte 64 kb/s frame
 * carrying an "Info" tag whose frame count (frames = payload frames after head, i.e.
 * (fileSize - kHeadLen) / kFrameLen) and byte count (fileSize) are patched in.
 *
 * @param dest      Destination buffer; must hold at least kHeadLen bytes.
 * @param fileSize  Total MP3 file size in bytes, including the head.
 */
inline void writeHead(uint8_t* dest, uint32_t fileSize) {
    // ID3v2.3 header, size=34 (synchsafe) → total ID3 = 44
    static const uint8_t kId3[44] = {
        0x49, 0x44, 0x33, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x54, 0x53, 0x53, 0x45, 0x00, 0x00,
        0x00, 0x0e, 0x00, 0x00, 0x03, 0x4c, 0x61, 0x76, 0x66, 0x36, 0x31, 0x2e, 0x37, 0x2e, 0x31, 0x30,
        0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    memcpy(dest, kId3, 44);

    // First audio frame: reuse structure from lame Info @ 64 kb/s (208 B), patch tag fields.
    memset(dest + 44, 0, 208);
    dest[44] = 0xff;
    dest[45] = 0xfb;
    dest[46] = 0x50;  // 64 kb/s, 44.1 kHz, pad=0
    dest[47] = 0x00;
    // Side info skipped (zeros ok enough for Info-only frame); place "Info" at +36 like LAME.
    const uint32_t tagOff = 44 + 36;
    dest[tagOff + 0] = 'I';
    dest[tagOff + 1] = 'n';
    dest[tagOff + 2] = 'f';
    dest[tagOff + 3] = 'o';
    writeBe32(dest + tagOff + 4, 0x3);  // frames + bytes
    uint32_t payload = fileSize > kHeadLen ? (fileSize - kHeadLen) : 0;
    uint32_t nFrames = payload / kFrameLen;
    writeBe32(dest + tagOff + 8, nFrames);
    writeBe32(dest + tagOff + 12, fileSize);
}

/**
 * @brief Return the body offset of the first frame that carries marker index `idx` (0-based).
 *
 * The body starts after the kHeadLen head; the offset is idx × kMarkerIntervalBytes aligned
 * down to a kFrameLen frame boundary.
 *
 * @param idx  Marker index (0-based).
 * @return Byte offset relative to the start of the body (not the file).
 */
inline uint32_t markerBodyOff(uint16_t idx) {
    uint32_t raw = (uint32_t)idx * kMarkerIntervalBytes;
    return (raw / kFrameLen) * kFrameLen;
}

/**
 * @brief Return the absolute file offset of marker `idx` (start of stamped frame).
 * @param idx       Marker index (0-based).
 * @param fileSize  Total MP3 file size in bytes, including the head.
 * @return File offset of the stamped frame, or 0 if past EOF or the file is not larger than kHeadLen.
 */
inline uint32_t markerFileOff(uint16_t idx, uint32_t fileSize) {
    if (fileSize <= kHeadLen) return 0;
    uint32_t abs = kHeadLen + markerBodyOff(idx);
    return abs < fileSize ? abs : 0;
}

/**
 * @brief Return the number of M3 markers contained in a file of fileSize bytes.
 *
 * Marker 0 sits at the start of the body, so any body yields at least one marker.
 *
 * @param fileSize  Total MP3 file size in bytes, including the head.
 * @return body / kMarkerIntervalBytes + 1, or 0 if fileSize <= kHeadLen.
 */
inline uint16_t markerCount(uint32_t fileSize) {
    if (fileSize <= kHeadLen || kMarkerIntervalBytes == 0) return 0;
    uint32_t body = fileSize - kHeadLen;
    return (uint16_t)(body / kMarkerIntervalBytes + 1);
}

/**
 * @brief Fill a read buffer with MP3 silence for an absolute file offset (with M3 stamps).
 *
 * Bytes below kHeadLen come from writeHead(), the rest tiles kFrame; the first frame of each
 * marker window gets "PDMK" + 16-bit big-endian marker index at frame bytes 4..9.
 * Bytes at or beyond fileSize are zero-filled.
 *
 * @param out       Destination buffer.
 * @param outLen    Number of bytes to produce in out.
 * @param fileOff   Absolute file offset corresponding to out[0].
 * @param fileSize  Total MP3 file size in bytes, including the head.
 * @note Uses a kHeadLen (252 B) stack buffer when the range touches the head.
 */
inline void fill(uint8_t* out, uint32_t outLen, uint32_t fileOff, uint32_t fileSize) {
    uint32_t o = 0;
    while (o < outLen) {
        uint32_t abs = fileOff + o;
        if (abs >= fileSize) {
            memset(out + o, 0, outLen - o);
            break;
        }
        uint32_t n = outLen - o;
        if (abs + n > fileSize) n = fileSize - abs;

        if (abs < kHeadLen) {
            uint8_t head[kHeadLen];
            writeHead(head, fileSize);
            uint32_t take = kHeadLen - abs;
            if (take > n) take = n;
            memcpy(out + o, head + abs, take);
            o += take;
            continue;
        }

        uint32_t body = abs - kHeadLen;
        uint32_t into = body % kFrameLen;
        uint32_t take = kFrameLen - into;
        if (take > n) take = n;
        memcpy(out + o, kFrame + into, take);

        // Stamp first frame of each ~30 s window: keep sync (0..3), write PDMK+idx at 4..9.
        // Note: markerBodyOff(i) aligns down to a frame, so it can be < i*INTERVAL —
        // floor(frameBody0/INTERVAL) alone misses those frames (use guess and guess+1).
        const uint32_t frameBody0 = body - into;
        uint16_t stampIdx = 0;
        bool isMarker = false;
        {
            const uint16_t guess = (uint16_t)(frameBody0 / kMarkerIntervalBytes);
            for (uint16_t d = 0; d < 2; d++) {
                const uint16_t cand = (uint16_t)(guess + d);
                if (markerBodyOff(cand) == frameBody0) {
                    stampIdx = cand;
                    isMarker = true;
                    break;
                }
            }
        }
        if (isMarker) {
            for (uint32_t i = 0; i < take; i++) {
                const uint32_t fr = into + i;  // 0..kFrameLen
                uint8_t v = 0;
                bool stamp = true;
                if (fr == 4) v = kMarkerMagic0;
                else if (fr == 5) v = kMarkerMagic1;
                else if (fr == 6) v = kMarkerMagic2;
                else if (fr == 7) v = kMarkerMagic3;
                else if (fr == 8) v = (uint8_t)(stampIdx >> 8);
                else if (fr == 9) v = (uint8_t)(stampIdx & 0xff);
                else stamp = false;
                if (stamp) out[o + i] = v;
            }
        }
        o += take;
    }
}

}  // namespace Mp3Silence
