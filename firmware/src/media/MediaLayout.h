#pragma once

#include <Arduino.h>

/**
 * FAT geometry + file/cluster map of the medium the probe presents (0.2.31).
 *
 * Filled either by parsing a FAT12/16 image (demo_v1) or by SynthFat from a spec.
 * Used for (a) LBA classification of every read (B21: no hardcoded bands) and
 * (b) on-the-fly sector synthesis (FAT entries, directories, file data).
 */
class MediaLayout {
public:
    static constexpr uint8_t kMaxNodes = 64;
    static constexpr uint16_t kMaxExtents = 384;
    static constexpr uint8_t kNoParent = 0xFF;
    static constexpr int16_t kNoExtent = -1;

    enum NodeFlags : uint8_t {
        kDir = 0x01,
        kId3v1 = 0x02,    // 128-byte ID3v1 tag at EOF
        kXing = 0x04,     // Xing/Info header frame
        kVbr = 0x08,      // alternating bitrates (Xing says "Xing")
        kStamp = 0x10,    // per-file/per-frame stamp in ancillary bytes
        kNoId3v2 = 0x20,  // no ID3v2 header at all
    };

    enum class Region : uint8_t { Boot, Reserved, Fat, Root, Dir, File, Free, Beyond };

    struct Node {
        char name[13] = {0};   // 8.3 "TRACK01.MP3"
        uint8_t parent = kNoParent;
        uint8_t flags = 0;
        uint8_t seed = 0;      // content variant (SEQ-CACHE)
        uint16_t kbps = 0;     // CBR / VBR base bitrate
        uint32_t size = 0;     // bytes (dirs: allocated bytes)
        uint32_t durMs = 0;    // nominal duration (synth)
        uint32_t frames = 0;   // MP3 frames (synth)
        uint32_t apicBytes = 0;  // APIC picture slot (real cover + zero padding, or placeholder)
        uint8_t cover = 0;       // 0 = placeholder bytes, 1..CoverJpegs::kCount = real JPEG
        uint16_t firstCluster = 0;
        int16_t firstExtent = kNoExtent;
        uint16_t extentCount = 0;
    };

    /** Contiguous cluster run of one node; `next` = index of following extent or -1. */
    struct Extent {
        uint16_t first = 0;
        uint16_t count = 0;
        uint8_t node = 0;
        uint8_t pad = 0;
        int16_t next = kNoExtent;
        uint32_t fileCluster = 0;  // cluster index within the node
    };

    void clear();

    // Geometry
    uint8_t fatBits = 16;
    uint32_t sectorCount = 0;
    uint16_t reserved = 1;
    uint8_t fats = 2;
    uint32_t fatSectors = 0;
    uint16_t rootEntries = 512;
    uint32_t rootLba = 0;
    uint32_t rootSectors = 0;
    uint32_t dataLba = 0;
    uint8_t spc = 1;
    uint32_t clusterCount = 0;  // data clusters (cluster ids 2..clusterCount+1)
    uint32_t volumeId = 0;
    char label[12] = "HUPROBE";

    /**
     * ID3 texts in fixed slots (ID3v1 field width). Empty = default text. Fixed width keeps
     * the ID3v2 tag size constant, so texts can change live without moving audio data.
     */
    static constexpr uint8_t kTagLen = 30;
    struct Tags {
        char title[kTagLen + 1] = {0};
        char artist[kTagLen + 1] = {0};
        char album[kTagLen + 1] = {0};
    };

    Node nodes[kMaxNodes];
    Tags tags[kMaxNodes];
    uint8_t nodeCount = 0;
    Extent extents[kMaxExtents];
    uint16_t extentCount = 0;
    bool truncated = false;  // node/extent table full while building

    uint32_t clusterBytes() const { return (uint32_t)spc * 512u; }
    uint32_t clusterLba(uint32_t cluster) const { return dataLba + (cluster - 2) * spc; }
    void recomputeRegions();

    /** Binary search (extents are kept sorted by `first`). */
    int16_t findExtent(uint32_t cluster) const;
    /** FAT entry value for cluster (0 free, EOC = 0xFFFF / 0xFFF). */
    uint32_t fatEntry(uint32_t cluster) const;
    /** Appends a run for node, merging with the node's previous run if adjacent. */
    bool addExtent(uint8_t node, uint16_t first, uint16_t count);
    /** Sort extents by cluster and fix up `next` links + node.firstExtent. */
    void finalizeExtents();

    Region classify(uint32_t lba, int16_t* node, uint32_t* fileBlock) const;
    static const char* regionTag(Region r);
    static const char* regionKind(Region r);

    /** "DIR/SUB/NAME.MP3" (max n-1 chars). */
    void nodePath(uint8_t idx, char* out, size_t n) const;

    /** Parse a FAT12/16 image in memory (root + subdirectories). */
    bool parseImage(const uint8_t* img, uint32_t size);
};
