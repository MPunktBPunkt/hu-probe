#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "media/MediaLayout.h"

/**
 * Synthetic FAT16 medium (0.2.31, M1).
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

    struct FileSpec {
        char path[40] = {0};
        uint32_t durMs = 0;
        uint32_t sizeBytes = 0;
        uint16_t kbps = 0;
        uint8_t flags = 0;  // MediaLayout::NodeFlags
        uint8_t seed = 0;
        uint32_t apicBytes = 0;
        uint8_t cover = 0;
        uint8_t track = 0;
        uint16_t gapClusters = 0;
        bool frag = false;
        MediaLayout::Tags tags;
    };

    struct Spec {
        char id[24] = "custom";
        char label[48] = {0};
        uint16_t volumeMib = 0;  // 0 = auto
        uint8_t spc = 0;         // 0 = auto
        uint16_t reserved = 1;
        uint32_t fragmentKib = 0;
        char fsLabel[12] = "HUPROBE";
        uint32_t volumeId = 0x48500131;
        char usbSerial[16] = {0};
        FileSpec files[kMaxFiles];
        uint8_t fileCount = 0;
    };

    /** Parse JSON spec; err gets a short reason on failure. */
    static bool parseSpec(JsonVariantConst j, Spec& out, String* err);

    /**
     * JSON string → ID3 text slot (ISO-8859-1, max MediaLayout::kTagLen). UTF-8 Latin-1
     * characters (umlauts) are converted, others become '?'. Missing/null leaves dst as is.
     */
    static bool setTag(char* dst, JsonVariantConst v, String* err);

    /** Build geometry + node/extent map into layout. */
    bool build(const Spec& spec, MediaLayout& layout, String* err);

    /** Generate one 512-byte sector. */
    void sector(const MediaLayout& layout, uint32_t lba, uint8_t* out) const;

private:
    void bootSector(const MediaLayout& L, uint8_t* out) const;
    void fatSector(const MediaLayout& L, uint32_t lba, uint8_t* out) const;
    void dirSector(const MediaLayout& L, uint8_t dirNode, uint32_t firstEntry, uint8_t* out) const;
    static void dirEntry(uint8_t* e, const char* name, uint8_t attr, uint16_t cluster, uint32_t size);
    static int childAt(const MediaLayout& L, uint8_t parent, uint32_t k);
    static uint32_t childCount(const MediaLayout& L, uint8_t parent);
    bool tryGeometry(const Spec& spec, MediaLayout& L, uint32_t sectors, uint8_t spc) const;
    bool allocate(const Spec& spec, MediaLayout& L, String* err) const;
};
