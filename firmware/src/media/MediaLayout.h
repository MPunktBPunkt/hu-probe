#pragma once

/**
 * @file MediaLayout.h
 * @brief FAT12/16 geometry and node/cluster-extent map of the presented USB medium.
 */

#include <Arduino.h>

/**
 * @brief FAT geometry + file/cluster map of the medium the probe presents (0.2.31).
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

    /** @brief Bit flags in Node::flags (node type and synthesized MP3 variants). */
    enum NodeFlags : uint8_t {
        kDir = 0x01,      ///< node is a directory
        kId3v1 = 0x02,    ///< 128-byte ID3v1 tag at EOF
        kXing = 0x04,     ///< Xing/Info header frame
        kVbr = 0x08,      ///< alternating bitrates (Xing says "Xing")
        kStamp = 0x10,    ///< per-file/per-frame stamp in ancillary bytes
        kNoId3v2 = 0x20,  ///< no ID3v2 header at all
    };

    /** @brief Volume area an LBA falls into (result of classify()). */
    enum class Region : uint8_t { Boot, Reserved, Fat, Root, Dir, File, Free, Beyond };

    /** @brief One file or directory of the medium. */
    struct Node {
        char name[13] = {0};   ///< 8.3 "TRACK01.MP3"
        uint8_t parent = kNoParent;  ///< parent dir node index, kNoParent = root directory
        uint8_t flags = 0;     ///< NodeFlags bits
        uint8_t seed = 0;      ///< content variant (SEQ-CACHE)
        uint16_t kbps = 0;     ///< CBR / VBR base bitrate
        uint32_t size = 0;     ///< bytes (dirs: allocated bytes)
        uint32_t durMs = 0;    ///< nominal duration (synth)
        uint32_t frames = 0;   ///< MP3 frames (synth)
        uint32_t apicBytes = 0;  ///< APIC picture slot (real cover + zero padding, or placeholder)
        uint8_t cover = 0;       ///< 0 = placeholder bytes, 1..CoverJpegs::kCount = real JPEG
        uint8_t track = 0;       ///< 0 = no TRCK frame / plain ID3v1; else track number
        uint16_t firstCluster = 0;  ///< first data cluster (FAT cluster id >= 2)
        int16_t firstExtent = kNoExtent;  ///< index of the first extent in file order
        uint16_t extentCount = 0;  ///< number of extents of this node
    };

    /** @brief Contiguous cluster run of one node; `next` = index of following extent or -1. */
    struct Extent {
        uint16_t first = 0;  ///< first cluster id of the run
        uint16_t count = 0;  ///< number of clusters in the run
        uint8_t node = 0;    ///< owning node index
        uint8_t pad = 0;
        int16_t next = kNoExtent;
        uint32_t fileCluster = 0;  ///< cluster index within the node
    };

    /**
     * @brief Reset geometry to defaults (FAT16, 512 root entries, label "HUPROBE") and empty all
     *        node, tag and extent tables.
     */
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

    static constexpr uint8_t kTagLen = 30;

    /**
     * @brief ID3 texts in fixed slots (ID3v1 field width, kTagLen chars). Empty = default text.
     *
     * Fixed width keeps the ID3v2 tag size constant, so texts can change live without moving
     * audio data.
     */
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

    /** @brief Cluster size in bytes (sectors per cluster * 512). */
    uint32_t clusterBytes() const { return (uint32_t)spc * 512u; }

    /**
     * @brief Return the first LBA of a data cluster.
     *
     * @param cluster  FAT cluster id (>= 2; no range check).
     * @return LBA of the cluster's first sector.
     */
    uint32_t clusterLba(uint32_t cluster) const { return dataLba + (cluster - 2) * spc; }

    /**
     * @brief Derive rootLba, rootSectors, dataLba and clusterCount from reserved, fats,
     *        fatSectors, rootEntries, sectorCount and spc.
     */
    void recomputeRegions();

    /**
     * @brief Find the extent that contains a cluster.
     *
     * Binary search (extents are kept sorted by `first`).
     *
     * @param cluster  FAT cluster id.
     * @return Extent index, or kNoExtent if the cluster is free / not allocated.
     * @pre finalizeExtents() has sorted the extent table.
     */
    int16_t findExtent(uint32_t cluster) const;

    /**
     * @brief Synthesize the FAT entry value for a cluster from the extent table.
     *
     * Clusters 0/1 return the media descriptor / EOC reserved values.
     *
     * @param cluster  FAT cluster id.
     * @return FAT entry value for cluster: next cluster of the chain, 0 free, EOC = 0xFFFF / 0xFFF.
     */
    uint32_t fatEntry(uint32_t cluster) const;

    /**
     * @brief Append a cluster run for a node.
     *
     * Merges with the node's previous run if adjacent; otherwise adds a new extent and
     * updates the node's firstCluster/extentCount. Sets `truncated` when the table is full.
     *
     * @param node   Node index the run belongs to.
     * @param first  First cluster id of the run.
     * @param count  Number of clusters (0 is a no-op).
     * @return true if the run was stored or merged, false if the extent table is full.
     */
    bool addExtent(uint8_t node, uint16_t first, uint16_t count);

    /**
     * @brief Sort extents by cluster and fix up `next` links + node.firstExtent.
     *
     * Insertion sort, done once per media switch; links each node's extents in file order.
     */
    void finalizeExtents();

    /**
     * @brief Classify an LBA into its volume region (B21: no hardcoded bands).
     *
     * @param lba             Sector address to classify.
     * @param[out] node       Owning node index for Dir/File, else -1; may be nullptr.
     * @param[out] fileBlock  512-byte block index within the file for File, else 0; may be nullptr.
     * @return Region of the LBA (Beyond if lba >= sectorCount).
     */
    Region classify(uint32_t lba, int16_t* node, uint32_t* fileBlock) const;

    /**
     * @brief Return the upper-case short tag of a region for logs.
     *
     * @param r  Region to name.
     * @return "BOOT", "RSVD", "FAT", "ROOT", "DIR", "DATA", "FREE", "OOR" or "?".
     */
    static const char* regionTag(Region r);

    /**
     * @brief Return the lower-case kind of a region (root and subdirectories both map to "dir").
     *
     * @param r  Region to name.
     * @return "boot", "rsvd", "fat", "dir", "file", "free", "oor" or "?".
     */
    static const char* regionKind(Region r);

    /**
     * @brief Build the full path of a node, e.g. "DIR/SUB/NAME.MP3" (max n-1 chars).
     *
     * Longer paths are cut off at n-1 chars; at most 8 directory levels are followed.
     *
     * @param idx       Node index; out is empty if idx >= nodeCount.
     * @param[out] out  Destination buffer, always NUL-terminated if n > 0.
     * @param n         Size of out in bytes.
     */
    void nodePath(uint8_t idx, char* out, size_t n) const;

    /**
     * @brief Parse a FAT12/16 image in memory (root + subdirectories).
     *
     * Calls clear(), reads the BPB (512-byte sectors only), volume label, then walks all
     * directories breadth-first and builds nodes and extents from the FAT chains. LFN entries,
     * deleted entries and "."/".." are skipped. Sets `truncated` if the node table overflows.
     *
     * @param img   Image bytes; may be shorter than the volume (tail treated as zero).
     * @param size  Number of valid bytes in img.
     * @return true on success; false if img is null, smaller than 512 bytes, has a sector size
     *         other than 512, or ends before the data region.
     */
    bool parseImage(const uint8_t* img, uint32_t size);
};
