/**
 * @file SynthFat.cpp
 * @brief SynthFat spec parsing, FAT16 geometry/cluster allocation and per-LBA sector synthesis.
 */
#include "media/SynthFat.h"
#include "media/CoverJpegs.h"
#include "media/Mp3Gen.h"
#include <cstring>

namespace {

constexpr uint32_t kMinFat16Clusters = 4085;
constexpr uint32_t kMaxFat16Clusters = 65524;
// 2026-10-19 12:00:00 (FAT date/time)
constexpr uint16_t kFatDate = ((2026 - 1980) << 9) | (10 << 5) | 19;
constexpr uint16_t kFatTime = (12 << 11);

/**
 * @brief Write a 16-bit value little-endian.
 *
 * @param[out] p  Destination, 2 bytes.
 * @param v       Value to write.
 */
void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

/**
 * @brief Write a 32-bit value little-endian.
 *
 * @param[out] p  Destination, 4 bytes.
 * @param v       Value to write.
 */
void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/**
 * @brief Check whether an (upper-case) character is allowed in a FAT short name.
 *
 * @param c  Character to check.
 * @return true for A-Z, 0-9 and _-~!#$%&'()@^{}.
 */
bool validSfnChar(char c) {
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    return strchr("_-~!#$%&'()@^{}", c) != nullptr;
}

/**
 * @brief Convert a "name.ext" path component to uppercase 8.3 into out[13].
 *
 * @param comp      Path component (not NUL-terminated).
 * @param len       Length of comp in chars.
 * @param[out] out  Destination, 13 bytes; "NAME.EXT", NUL-terminated.
 * @return false if not representable (empty stem, stem > 8, ext > 3, two dots, invalid char).
 */
bool toSfn(const char* comp, size_t len, char* out) {
    const char* dot = nullptr;
    for (size_t i = 0; i < len; i++) {
        if (comp[i] == '.') {
            if (dot) return false;
            dot = comp + i;
        }
    }
    const size_t stemLen = dot ? (size_t)(dot - comp) : len;
    const size_t extLen = dot ? len - stemLen - 1 : 0;
    if (stemLen == 0 || stemLen > 8 || extLen > 3) return false;
    size_t p = 0;
    for (size_t i = 0; i < stemLen; i++) {
        char c = (char)toupper((unsigned char)comp[i]);
        if (!validSfnChar(c)) return false;
        out[p++] = c;
    }
    if (extLen) {
        out[p++] = '.';
        for (size_t i = 0; i < extLen; i++) {
            char c = (char)toupper((unsigned char)dot[1 + i]);
            if (!validSfnChar(c)) return false;
            out[p++] = c;
        }
    }
    out[p] = 0;
    return true;
}

/**
 * @brief Expand "NAME.EXT" to the 11-byte space-padded directory entry form.
 *
 * @param name      8.3 name with optional extension.
 * @param[out] out  Destination, 11 bytes.
 */
void sfn11(const char* name, uint8_t* out) {
    memset(out, ' ', 11);
    const char* dot = strchr(name, '.');
    const size_t stem = dot ? (size_t)(dot - name) : strlen(name);
    memcpy(out, name, stem > 8 ? 8 : stem);
    if (dot) {
        size_t ext = strlen(dot + 1);
        memcpy(out + 8, dot + 1, ext > 3 ? 3 : ext);
    }
}

/**
 * @brief Parse a volume id given as number or string ("0x48500B01", decimal or octal).
 *
 * @param v    JSON value.
 * @param def  Value returned if v is neither a number nor a string.
 * @return Parsed volume id (strtoul base 0; invalid strings yield 0).
 */
uint32_t parseVolumeId(JsonVariantConst v, uint32_t def) {
    if (v.is<uint32_t>()) return v.as<uint32_t>();
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        return (uint32_t)strtoul(s, nullptr, 0);
    }
    return def;
}

}  // namespace

bool SynthFat::setTag(char* dst, JsonVariantConst v, String* err) {
    if (v.isNull()) return true;
    if (!v.is<const char*>()) {
        if (err) *err = "tag_not_string";
        return false;
    }
    const uint8_t* s = (const uint8_t*)v.as<const char*>();
    char tmp[MediaLayout::kTagLen + 1];
    size_t n = 0;
    while (*s) {
        if (n >= MediaLayout::kTagLen) {
            if (err) *err = "tag_le_30";
            return false;
        }
        uint8_t c = *s++;
        if (c < 0x80) {
            tmp[n++] = (c < 0x20) ? '?' : (char)c;
        } else if ((c == 0xC2 || c == 0xC3) && (*s & 0xC0) == 0x80) {
            tmp[n++] = (char)(((c & 0x03) << 6) | (*s++ & 0x3F));
        } else {
            while ((*s & 0xC0) == 0x80) s++;
            tmp[n++] = '?';
        }
    }
    tmp[n] = 0;
    memcpy(dst, tmp, n + 1);
    return true;
}

bool SynthFat::parseSpec(JsonVariantConst j, Spec& out, String* err) {
    // Spec is ~7.6 KiB — never `out = Spec()` (temporary on the ~8 KiB Arduino loop stack → panic).
    memset(&out, 0, sizeof(out));
    snprintf(out.id, sizeof(out.id), "custom");
    snprintf(out.fsLabel, sizeof(out.fsLabel), "HUPROBE");
    out.volumeId = 0x48500131;
    out.reserved = 1;
    if (!j.is<JsonObjectConst>()) {
        if (err) *err = "spec_not_object";
        return false;
    }
    if (j["id"].is<const char*>()) snprintf(out.id, sizeof(out.id), "%s", j["id"].as<const char*>());
    if (j["label"].is<const char*>())
        snprintf(out.label, sizeof(out.label), "%s", j["label"].as<const char*>());
    out.volumeMib = j["volume_mib"] | (uint16_t)0;
    out.spc = j["spc"] | (uint8_t)0;
    if (out.spc && (out.spc > 64 || (out.spc & (out.spc - 1)))) {
        if (err) *err = "spc_not_pow2_le64";
        return false;
    }
    out.reserved = j["reserved"] | (uint16_t)1;
    if (out.reserved < 1 || out.reserved > 64) {
        if (err) *err = "reserved_1_64";
        return false;
    }
    out.fragmentKib = j["fragment_kib"] | (uint32_t)0;
    if (j["fs_label"].is<const char*>()) {
        const char* s = j["fs_label"].as<const char*>();
        size_t n = 0;
        for (; s[n] && n < 11; n++) out.fsLabel[n] = (char)toupper((unsigned char)s[n]);
        out.fsLabel[n] = 0;
    }
    out.volumeId = parseVolumeId(j["volume_id"], out.volumeId);
    if (j["usb_serial"].is<const char*>())
        snprintf(out.usbSerial, sizeof(out.usbSerial), "%s", j["usb_serial"].as<const char*>());
    const uint16_t defKbps = j["kbps"] | (uint16_t)48;
    const bool defStamp = j["stamp"] | true;

    JsonArrayConst files = j["files"].as<JsonArrayConst>();
    if (files.isNull() || files.size() == 0) {
        if (err) *err = "no_files";
        return false;
    }
    for (JsonObjectConst f : files) {
        if (out.fileCount >= kMaxFiles) {
            if (err) *err = "too_many_files";
            return false;
        }
        FileSpec& fs = out.files[out.fileCount];
        const char* path = f["path"] | "";
        if (!path[0] || strlen(path) >= sizeof(fs.path)) {
            if (err) *err = "bad_path";
            return false;
        }
        snprintf(fs.path, sizeof(fs.path), "%s", path);
        const float durS = f["dur_s"] | 0.0f;
        fs.durMs = (uint32_t)(durS * 1000.0f + 0.5f);
        fs.sizeBytes = (f["size_kib"] | (uint32_t)0) * 1024u;
        if (!fs.durMs && !fs.sizeBytes) fs.durMs = 26000;
        fs.kbps = f["kbps"] | defKbps;
        if (fs.kbps < 32 || fs.kbps > 320) {
            if (err) *err = "kbps_32_320";
            return false;
        }
        uint8_t fl = 0;
        if (f["vbr"] | false) fl |= MediaLayout::kVbr;
        if (f["xing"] | false) fl |= MediaLayout::kXing;
        if (f["id3v1"] | false) fl |= MediaLayout::kId3v1;
        if (f["no_id3v2"] | false) fl |= MediaLayout::kNoId3v2;
        if (f["stamp"] | defStamp) fl |= MediaLayout::kStamp;
        fs.flags = fl;
        fs.seed = f["seed"] | (uint8_t)0;
        fs.apicBytes = (f["apic_kib"] | (uint32_t)0) * 1024u;
        if (fs.apicBytes > 2u * 1024u * 1024u) {
            if (err) *err = "apic_le_2mib";
            return false;
        }
        fs.cover = f["cover"] | (uint8_t)0;
        if (fs.cover > CoverJpegs::kCount) {
            if (err) *err = "cover_0_4";
            return false;
        }
        if ((fs.cover || (f["cover_slot"] | false)) && fs.apicBytes < CoverJpegs::slotBytes())
            fs.apicBytes = CoverJpegs::slotBytes();
        if (fs.apicBytes && (fl & MediaLayout::kNoId3v2)) {
            if (err) *err = "apic_needs_id3v2";
            return false;
        }
        if (!setTag(fs.tags.title, f["title"], err) || !setTag(fs.tags.artist, f["artist"], err) ||
            !setTag(fs.tags.album, f["album"], err))
            return false;
        fs.track = f["track"] | (uint8_t)0;
        fs.gapClusters = f["gap_clusters"] | (uint16_t)0;
        fs.frag = f["frag"] | false;
        out.fileCount++;
    }
    return true;
}

uint32_t SynthFat::childCount(const MediaLayout& L, uint8_t parent) {
    uint32_t n = 0;
    for (uint8_t i = 0; i < L.nodeCount; i++) {
        if (L.nodes[i].parent == parent) n++;
    }
    return n;
}

int SynthFat::childAt(const MediaLayout& L, uint8_t parent, uint32_t k) {
    for (uint8_t i = 0; i < L.nodeCount; i++) {
        if (L.nodes[i].parent != parent) continue;
        if (k == 0) return i;
        k--;
    }
    return -1;
}

bool SynthFat::tryGeometry(const Spec& spec, MediaLayout& L, uint32_t sectors, uint8_t spc) const {
    L.sectorCount = sectors;
    L.spc = spc;
    L.reserved = spec.reserved;
    L.fats = 2;
    L.rootEntries = 512;
    L.fatBits = 16;
    uint32_t fs = 1;
    for (int it = 0; it < 8; it++) {
        const uint32_t meta = (uint32_t)L.reserved + 2u * fs + 32u;
        if (sectors <= meta) return false;
        const uint32_t clusters = (sectors - meta) / spc;
        const uint32_t need = ((clusters + 2u) * 2u + 511u) / 512u;
        if (need <= fs) break;
        fs = need;
    }
    L.fatSectors = fs;
    L.recomputeRegions();
    return L.clusterCount >= kMinFat16Clusters && L.clusterCount <= kMaxFat16Clusters;
}

bool SynthFat::allocate(const Spec& spec, MediaLayout& L, String* err) const {
    const uint32_t cb = L.clusterBytes();
    uint32_t cursor = 2;
    L.extentCount = 0;
    for (uint8_t i = 0; i < L.nodeCount; i++) {
        L.nodes[i].extentCount = 0;
        L.nodes[i].firstExtent = MediaLayout::kNoExtent;
    }
    // Directories first (small, contiguous) — mirrors a freshly formatted stick.
    for (uint8_t i = 0; i < L.nodeCount; i++) {
        MediaLayout::Node& n = L.nodes[i];
        if (!(n.flags & MediaLayout::kDir)) continue;
        const uint32_t bytes = (childCount(L, i) + 2u) * 32u;
        const uint32_t cl = (bytes + cb - 1) / cb;
        if (!L.addExtent(i, (uint16_t)cursor, (uint16_t)cl)) break;
        n.size = cl * cb;
        cursor += cl;
    }
    // Files in spec order; frag group is interleaved at the first frag file.
    bool fragDone = false;
    for (uint8_t f = 0; f < spec.fileCount; f++) {
        const FileSpec& fs = spec.files[f];
        cursor += fs.gapClusters;
        // Node index of file f: nth non-dir node in creation order.
        int node = -1;
        for (uint8_t i = 0, seen = 0; i < L.nodeCount; i++) {
            if (L.nodes[i].flags & MediaLayout::kDir) continue;
            if (seen++ == f) {
                node = i;
                break;
            }
        }
        if (node < 0) continue;
        const uint32_t cl = (L.nodes[node].size + cb - 1) / cb;
        if (fs.frag && spec.fragmentKib > 0) {
            if (fragDone) continue;
            fragDone = true;
            uint32_t remaining[kMaxFiles] = {0};
            int nodesOf[kMaxFiles];
            uint8_t nf = 0;
            uint32_t total = 0;
            for (uint8_t g = 0, seen = 0; g < L.nodeCount && nf < kMaxFiles; g++) {
                if (L.nodes[g].flags & MediaLayout::kDir) continue;
                const uint8_t specIdx = seen++;
                if (specIdx >= spec.fileCount || !spec.files[specIdx].frag) continue;
                nodesOf[nf] = g;
                remaining[nf] = (L.nodes[g].size + cb - 1) / cb;
                total += remaining[nf];
                nf++;
            }
            uint32_t chunk = (spec.fragmentKib * 1024u + cb - 1) / cb;
            if (chunk == 0) chunk = 1;
            const uint32_t avail = (MediaLayout::kMaxExtents > L.extentCount + 8)
                                       ? (MediaLayout::kMaxExtents - L.extentCount - 8)
                                       : 1;
            const uint32_t room = (avail > nf + 1u) ? (avail - nf) : 1u;
            if ((total + chunk - 1) / chunk > room) chunk = (total + room - 1) / room;
            bool any = true;
            while (any) {
                any = false;
                for (uint8_t k = 0; k < nf; k++) {
                    if (!remaining[k]) continue;
                    const uint32_t take = remaining[k] < chunk ? remaining[k] : chunk;
                    if (!L.addExtent((uint8_t)nodesOf[k], (uint16_t)cursor, (uint16_t)take)) {
                        if (err) *err = "too_many_extents";
                        return false;
                    }
                    cursor += take;
                    remaining[k] -= take;
                    any = true;
                }
            }
            continue;
        }
        if (!L.addExtent((uint8_t)node, (uint16_t)cursor, (uint16_t)cl)) {
            if (err) *err = "too_many_extents";
            return false;
        }
        cursor += cl;
    }
    if (cursor - 2 > L.clusterCount) {
        if (err) *err = "out_of_space";
        return false;
    }
    L.finalizeExtents();
    return true;
}

bool SynthFat::build(const Spec& spec, MediaLayout& L, String* err) {
    L.clear();
    snprintf(L.label, sizeof(L.label), "%s", spec.fsLabel);
    L.volumeId = spec.volumeId;
    // Nodes: directories from path components, then the file itself.
    for (uint8_t f = 0; f < spec.fileCount; f++) {
        const FileSpec& fs = spec.files[f];
        uint8_t parent = MediaLayout::kNoParent;
        const char* p = fs.path;
        while (*p) {
            const char* slash = strchr(p, '/');
            const size_t len = slash ? (size_t)(slash - p) : strlen(p);
            char name[13];
            if (!toSfn(p, len, name)) {
                if (err) *err = String("bad_name:") + fs.path;
                return false;
            }
            if (slash) {
                int found = -1;
                for (uint8_t i = 0; i < L.nodeCount; i++) {
                    if ((L.nodes[i].flags & MediaLayout::kDir) && L.nodes[i].parent == parent &&
                        strcmp(L.nodes[i].name, name) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found < 0) {
                    if (L.nodeCount >= MediaLayout::kMaxNodes) {
                        if (err) *err = "too_many_nodes";
                        return false;
                    }
                    found = L.nodeCount++;
                    MediaLayout::Node& d = L.nodes[found];
                    snprintf(d.name, sizeof(d.name), "%s", name);
                    d.parent = parent;
                    d.flags = MediaLayout::kDir;
                }
                parent = (uint8_t)found;
                p = slash + 1;
            } else {
                if (L.nodeCount >= MediaLayout::kMaxNodes) {
                    if (err) *err = "too_many_nodes";
                    return false;
                }
                for (uint8_t i = 0; i < L.nodeCount; i++) {
                    if (L.nodes[i].parent == parent && strcmp(L.nodes[i].name, name) == 0) {
                        if (err) *err = String("duplicate:") + fs.path;
                        return false;
                    }
                }
                MediaLayout::Node& n = L.nodes[L.nodeCount++];
                snprintf(n.name, sizeof(n.name), "%s", name);
                n.parent = parent;
                n.flags = fs.flags;
                n.seed = fs.seed;
                n.kbps = fs.kbps;
                n.durMs = fs.durMs;
                n.size = fs.sizeBytes;
                n.apicBytes = fs.apicBytes;
                n.cover = fs.cover;
                n.track = fs.track;
                L.tags[L.nodeCount - 1] = fs.tags;
                Mp3Gen::finalizeNode(n);
                break;
            }
        }
    }
    for (uint8_t i = 0; i < L.nodeCount; i++) {
        if (childCount(L, i) + 2u > 2048u) {
            if (err) *err = "dir_too_large";
            return false;
        }
    }
    if (childCount(L, MediaLayout::kNoParent) + 1u > 512u) {
        if (err) *err = "root_full";
        return false;
    }

    const uint8_t spcList[] = {1, 2, 4, 8, 16, 32, 64};
    for (uint8_t si = 0; si < sizeof(spcList); si++) {
        const uint8_t spc = spcList[si];
        if (spec.spc && spc != spec.spc) continue;
        const uint32_t cb = (uint32_t)spc * 512u;
        uint32_t need = 0;
        for (uint8_t i = 0; i < L.nodeCount; i++) {
            const MediaLayout::Node& n = L.nodes[i];
            uint32_t bytes = (n.flags & MediaLayout::kDir) ? (childCount(L, i) + 2u) * 32u : n.size;
            need += (bytes + cb - 1) / cb;
        }
        for (uint8_t f = 0; f < spec.fileCount; f++) need += spec.files[f].gapClusters;
        const bool fixedVol = spec.volumeMib != 0;
        // Auto volume starts at 16 MiB so FAT16 stays above the 4085-cluster floor.
        // Fixed volumes that are too small for that floor are grown (still FAT16).
        uint32_t sectors = fixedVol ? (uint32_t)spec.volumeMib * 2048u : 32768u;
        const uint32_t want = need > kMinFat16Clusters ? need : kMinFat16Clusters;
        bool ok = false;
        for (int it = 0; it < 8; it++) {
            ok = tryGeometry(spec, L, sectors, spc);
            if (ok && L.clusterCount >= want && L.clusterCount <= kMaxFat16Clusters) break;
            if (L.clusterCount > kMaxFat16Clusters) break;
            const uint32_t grow = (want > L.clusterCount)
                                      ? (want - L.clusterCount) * spc + 64u
                                      : (kMinFat16Clusters * spc + 2048u);
            sectors += grow;
            sectors = (sectors + 2047u) / 2048u * 2048u;
            ok = false;
        }
        if (!ok || L.clusterCount < need) continue;
        if (allocate(spec, L, err)) return true;
        if (err && *err == "too_many_extents") return false;
    }
    if (err && !err->length()) *err = spec.spc ? "geometry_spc_volume" : "geometry";
    return false;
}

void SynthFat::dirEntry(uint8_t* e, const char* name, uint8_t attr, uint16_t cluster,
                        uint32_t size) {
    memset(e, 0, 32);
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        memset(e, ' ', 11);
        memcpy(e, name, strlen(name));
    } else {
        sfn11(name, e);
    }
    e[11] = attr;
    put16(e + 14, kFatTime);
    put16(e + 16, kFatDate);
    put16(e + 18, kFatDate);
    put16(e + 22, kFatTime);
    put16(e + 24, kFatDate);
    put16(e + 26, cluster);
    put32(e + 28, size);
}

void SynthFat::bootSector(const MediaLayout& L, uint8_t* b) const {
    memset(b, 0, 512);
    b[0] = 0xEB;
    b[1] = 0x3C;
    b[2] = 0x90;
    memcpy(b + 3, "MSWIN4.1", 8);
    put16(b + 11, 512);
    b[13] = L.spc;
    put16(b + 14, L.reserved);
    b[16] = L.fats;
    put16(b + 17, L.rootEntries);
    if (L.sectorCount < 65536) {
        put16(b + 19, (uint16_t)L.sectorCount);
    } else {
        put32(b + 32, L.sectorCount);
    }
    b[21] = 0xF8;
    put16(b + 22, (uint16_t)L.fatSectors);
    put16(b + 24, 63);
    put16(b + 26, 255);
    b[36] = 0x80;
    b[38] = 0x29;
    put32(b + 39, L.volumeId);
    memset(b + 43, ' ', 11);
    memcpy(b + 43, L.label, strnlen(L.label, 11));
    memcpy(b + 54, "FAT16   ", 8);
    b[510] = 0x55;
    b[511] = 0xAA;
}

void SynthFat::fatSector(const MediaLayout& L, uint32_t lba, uint8_t* out) const {
    const uint32_t idx = (lba - L.reserved) % L.fatSectors;
    const uint32_t base = idx * 256u;
    const uint32_t limit = L.clusterCount + 2u;
    for (uint32_t j = 0; j < 256; j++) {
        const uint32_t c = base + j;
        put16(out + j * 2, (uint16_t)((c < limit) ? L.fatEntry(c) : 0));
    }
}

void SynthFat::dirSector(const MediaLayout& L, uint8_t dirNode, uint32_t firstEntry,
                         uint8_t* out) const {
    memset(out, 0, 512);
    for (uint32_t i = 0; i < 16; i++) {
        const uint32_t e = firstEntry + i;
        uint8_t* p = out + i * 32;
        uint32_t k;
        if (dirNode == MediaLayout::kNoParent) {
            if (e == 0) {
                memset(p, ' ', 11);
                memcpy(p, L.label, strnlen(L.label, 11));
                p[11] = 0x08;
                put16(p + 22, kFatTime);
                put16(p + 24, kFatDate);
                continue;
            }
            k = e - 1;
        } else {
            const MediaLayout::Node& self = L.nodes[dirNode];
            if (e == 0) {
                dirEntry(p, ".", 0x10, self.firstCluster, 0);
                continue;
            }
            if (e == 1) {
                const uint16_t pc =
                    (self.parent == MediaLayout::kNoParent) ? 0 : L.nodes[self.parent].firstCluster;
                dirEntry(p, "..", 0x10, pc, 0);
                continue;
            }
            k = e - 2;
        }
        const int c = childAt(L, dirNode, k);
        if (c < 0) break;
        const MediaLayout::Node& n = L.nodes[c];
        const bool isDir = (n.flags & MediaLayout::kDir) != 0;
        dirEntry(p, n.name, isDir ? 0x10 : 0x20, n.firstCluster, isDir ? 0 : n.size);
    }
}

void SynthFat::sector(const MediaLayout& L, uint32_t lba, uint8_t* out) const {
    if (lba == 0) {
        bootSector(L, out);
        return;
    }
    if (lba < L.reserved || lba >= L.sectorCount) {
        memset(out, 0, 512);
        return;
    }
    if (lba < L.rootLba) {
        fatSector(L, lba, out);
        return;
    }
    if (lba < L.dataLba) {
        dirSector(L, MediaLayout::kNoParent, (lba - L.rootLba) * 16u, out);
        return;
    }
    const uint32_t rel = lba - L.dataLba;
    const uint32_t cluster = rel / L.spc + 2;
    const int16_t ei = L.findExtent(cluster);
    if (ei == MediaLayout::kNoExtent) {
        memset(out, 0, 512);
        return;
    }
    const MediaLayout::Extent& e = L.extents[ei];
    const uint32_t blk = (e.fileCluster + (cluster - e.first)) * L.spc + rel % L.spc;
    const MediaLayout::Node& n = L.nodes[e.node];
    if (n.flags & MediaLayout::kDir) {
        dirSector(L, e.node, blk * 16u, out);
        return;
    }
    Mp3Gen::fill(n, &L.tags[e.node], e.node, blk * 512u, out, 512);
}
