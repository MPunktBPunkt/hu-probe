#pragma once

/**
 * @file SynthFat.h
 * @brief Synthetic FAT16 medium generated per LBA from a JSON spec.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "media/MediaLayout.h"

/**
 * @brief Synthetic FAT16 medium (0.2.31, M1).
 *
 * Boot sector, FATs, directories and MP3 file data are generated per LBA at read
 * time — no flash image, volumes up to 2 GiB. A JSON spec defines files (path,
 * duration or size, bitrate, header variants, seed), subdirectories, cluster size,
 * fragmentation, gaps and identity (USB serial, FS label, volume id).
 *
 * Spec (all keys optional except files[].path):
 *   { "id","label","volume_mib","spc","reserved","fragment_kib","kbps","stamp",
 *     "fs_label","volume_id","usb_serial",
 *     "files":[{"path":"DIR/NAME.MP3","dur_s","size_kib","kbps","vbr","xing",
 *               "id3v1","apic_kib","no_id3v2","seed","gap_clusters","frag","stamp",
 *               "title","artist","album","cover","cover_slot","track"}] }
 * track 1..255 adds a TRCK frame (ID3v2) and the ID3v1.1 track byte; omitted = unchanged.
 * cover 1..4 embeds a real JPEG; cover or cover_slot reserves an APIC slot large enough
 * for every cover (apic_kib may enlarge it), so covers can be swapped live.
 */
class SynthFat {
public:
    static constexpr uint8_t kMaxFiles = 48;

    /** @brief One file entry of the spec (parsed files[] element). */
    struct FileSpec {
        char path[40] = {0};      ///< "DIR/NAME.MP3", components must be valid 8.3 names
        uint32_t durMs = 0;       ///< requested duration (preferred over sizeBytes)
        uint32_t sizeBytes = 0;   ///< requested size budget (size_kib * 1024)
        uint16_t kbps = 0;        ///< base bitrate 32..320 kbit/s
        uint8_t flags = 0;  ///< MediaLayout::NodeFlags
        uint8_t seed = 0;         ///< content variant
        uint32_t apicBytes = 0;   ///< APIC picture slot size in bytes
        uint8_t cover = 0;        ///< 0 = placeholder, 1..CoverJpegs::kCount = real JPEG
        uint8_t track = 0;        ///< 0 = no track number
        uint16_t gapClusters = 0; ///< free clusters left before this file
        bool frag = false;        ///< member of the interleaved fragmentation group
        MediaLayout::Tags tags;   ///< ID3 texts (empty = default)
    };

    /** @brief Parsed media spec (~7.6 KiB; allocate on the heap, not the loop stack). */
    struct Spec {
        char id[24] = "custom";   ///< variant id
        char label[48] = {0};     ///< human-readable label
        uint16_t volumeMib = 0;  ///< 0 = auto
        uint8_t spc = 0;         ///< 0 = auto
        uint16_t reserved = 1;   ///< reserved sectors 1..64
        uint32_t fragmentKib = 0; ///< fragment size for frag files, 0 = no fragmentation
        char fsLabel[12] = "HUPROBE";  ///< volume label (upper case, max 11 chars)
        uint32_t volumeId = 0x48500131; ///< FAT volume serial number
        char usbSerial[16] = {0}; ///< USB serial override, "" = default
        FileSpec files[kMaxFiles];
        uint8_t fileCount = 0;
    };

    /**
     * @brief Parse JSON spec; err gets a short reason on failure.
     *
     * Validates spc (power of 2, <= 64), reserved (1..64), kbps (32..320), apic_kib (<= 2 MiB),
     * cover (0..4) and tag lengths. Files without dur_s and size_kib default to 26 s.
     *
     * @param j         Spec JSON object (format see class description).
     * @param[out] out  Parsed spec; cleared with memset first.
     * @param[out] err  Short reason such as "no_files", "bad_path", "kbps_32_320"; may be nullptr.
     * @return true if the spec is valid.
     */
    static bool parseSpec(JsonVariantConst j, Spec& out, String* err);

    /**
     * @brief Convert a JSON string to an ID3 text slot (ISO-8859-1, max MediaLayout::kTagLen).
     *
     * UTF-8 Latin-1 characters (umlauts) are converted, others become '?' (as do control
     * characters). Missing/null leaves dst as is.
     *
     * @param[out] dst  Slot of at least MediaLayout::kTagLen + 1 bytes; only written on success.
     * @param v         JSON value (string or null).
     * @param[out] err  "tag_not_string" or "tag_le_30" on failure; may be nullptr.
     * @return true if v is null or was stored, false on a non-string or too-long text.
     */
    static bool setTag(char* dst, JsonVariantConst v, String* err);

    /**
     * @brief Build geometry + node/extent map into layout.
     *
     * Creates directory and file nodes from the paths, finalizes MP3 sizes, then tries
     * cluster sizes 1..64 sectors (or only spec.spc) and grows the volume until FAT16 fits
     * (auto volume starts at 16 MiB), and allocates clusters.
     *
     * @param spec          Parsed spec.
     * @param[out] layout   Layout to fill (cleared first).
     * @param[out] err      Reason on failure ("bad_name:...", "duplicate:...", "too_many_nodes",
     *                      "dir_too_large", "root_full", "too_many_extents", "geometry",
     *                      "geometry_spc_volume", ...); may be nullptr.
     * @return true if the layout is complete.
     */
    bool build(const Spec& spec, MediaLayout& layout, String* err);

    /**
     * @brief Generate one 512-byte sector.
     *
     * Boot sector, FAT, root/subdirectory entries or MP3 file data; reserved, free and
     * out-of-range sectors are zero.
     *
     * @param layout    Layout built by build().
     * @param lba       Sector address.
     * @param[out] out  Destination, 512 bytes.
     */
    void sector(const MediaLayout& layout, uint32_t lba, uint8_t* out) const;

private:
    /**
     * @brief Generate the FAT16 boot sector (BPB, volume id, label, 0x55AA signature).
     *
     * @param L         Layout providing the geometry.
     * @param[out] out  Destination, 512 bytes.
     */
    void bootSector(const MediaLayout& L, uint8_t* out) const;

    /**
     * @brief Generate one FAT16 sector (256 entries); both FAT copies map to the same content.
     *
     * @param L         Layout providing the extent map.
     * @param lba       Sector inside the FAT area (reserved..rootLba-1).
     * @param[out] out  Destination, 512 bytes.
     */
    void fatSector(const MediaLayout& L, uint32_t lba, uint8_t* out) const;

    /**
     * @brief Generate one directory sector (16 entries of 32 bytes).
     *
     * Root: entry 0 is the volume label. Subdirectory: entries 0/1 are "." and "..".
     * Remaining entries are the node's children in node order.
     *
     * @param L           Layout providing the nodes.
     * @param dirNode     Directory node index, or MediaLayout::kNoParent for the root.
     * @param firstEntry  Index of the first directory entry in this sector.
     * @param[out] out    Destination, 512 bytes.
     */
    void dirSector(const MediaLayout& L, uint8_t dirNode, uint32_t firstEntry, uint8_t* out) const;

    /**
     * @brief Write one 32-byte short-name directory entry with the fixed timestamp 2026-10-19 12:00.
     *
     * @param[out] e  Destination, 32 bytes.
     * @param name    "NAME.EXT", "." or "..".
     * @param attr    FAT attribute byte (0x10 dir, 0x20 archive).
     * @param cluster First cluster (0 = root for "..").
     * @param size    File size in bytes (0 for directories).
     */
    static void dirEntry(uint8_t* e, const char* name, uint8_t attr, uint16_t cluster, uint32_t size);

    /**
     * @brief Return the k-th child of a directory in node order.
     *
     * @param L       Layout providing the nodes.
     * @param parent  Directory node index, or MediaLayout::kNoParent for the root.
     * @param k       0-based child index.
     * @return Node index, or -1 if parent has fewer than k+1 children.
     */
    static int childAt(const MediaLayout& L, uint8_t parent, uint32_t k);

    /**
     * @brief Count the direct children of a directory.
     *
     * @param L       Layout providing the nodes.
     * @param parent  Directory node index, or MediaLayout::kNoParent for the root.
     * @return Number of nodes whose parent is parent.
     */
    static uint32_t childCount(const MediaLayout& L, uint8_t parent);

    /**
     * @brief Set a FAT16 geometry for a volume size and cluster size and check it.
     *
     * Iterates the FAT size until it covers all clusters (2 FATs, 512 root entries).
     *
     * @param spec           Spec providing the reserved sector count.
     * @param[in,out] L      Layout whose geometry fields are set.
     * @param sectors        Volume size in 512-byte sectors.
     * @param spc            Sectors per cluster.
     * @return true if the cluster count is within the FAT16 range 4085..65524.
     */
    bool tryGeometry(const Spec& spec, MediaLayout& L, uint32_t sectors, uint8_t spc) const;

    /**
     * @brief Allocate clusters for all nodes and finalize the extent map.
     *
     * Directories first (small, contiguous), then files in spec order with their
     * gap_clusters; the frag group is interleaved in fragment_kib chunks at the first frag
     * file (chunk enlarged if the extent table would overflow).
     *
     * @param spec      Parsed spec (gaps, frag flags, fragment size).
     * @param[in,out] L Layout with nodes and geometry set.
     * @param[out] err  "too_many_extents" or "out_of_space"; may be nullptr.
     * @return true if everything fits.
     */
    bool allocate(const Spec& spec, MediaLayout& L, String* err) const;
};
