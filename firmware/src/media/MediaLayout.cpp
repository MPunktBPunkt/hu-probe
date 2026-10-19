#include "media/MediaLayout.h"
#include <cstring>

static uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void MediaLayout::clear() {
    fatBits = 16;
    sectorCount = 0;
    reserved = 1;
    fats = 2;
    fatSectors = 0;
    rootEntries = 512;
    rootLba = rootSectors = dataLba = 0;
    spc = 1;
    clusterCount = 0;
    volumeId = 0;
    snprintf(label, sizeof(label), "HUPROBE");
    for (uint8_t i = 0; i < kMaxNodes; i++) {
        nodes[i] = Node();
        tags[i] = Tags();
    }
    nodeCount = 0;
    extentCount = 0;
    truncated = false;
}

void MediaLayout::recomputeRegions() {
    rootLba = reserved + (uint32_t)fats * fatSectors;
    rootSectors = ((uint32_t)rootEntries * 32u + 511u) / 512u;
    dataLba = rootLba + rootSectors;
    clusterCount = (sectorCount > dataLba) ? (sectorCount - dataLba) / spc : 0;
}

int16_t MediaLayout::findExtent(uint32_t cluster) const {
    int lo = 0;
    int hi = (int)extentCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const Extent& e = extents[mid];
        if (cluster < e.first) {
            hi = mid - 1;
        } else if (cluster >= (uint32_t)e.first + e.count) {
            lo = mid + 1;
        } else {
            return (int16_t)mid;
        }
    }
    return kNoExtent;
}

uint32_t MediaLayout::fatEntry(uint32_t cluster) const {
    const uint32_t eoc = (fatBits == 12) ? 0xFFFu : 0xFFFFu;
    if (cluster == 0) return (fatBits == 12) ? 0xFF8u : 0xFFF8u;
    if (cluster == 1) return eoc;
    int16_t i = findExtent(cluster);
    if (i == kNoExtent) return 0;
    const Extent& e = extents[i];
    if (cluster + 1 < (uint32_t)e.first + e.count) return cluster + 1;
    if (e.next != kNoExtent) return extents[e.next].first;
    return eoc;
}

bool MediaLayout::addExtent(uint8_t node, uint16_t first, uint16_t count) {
    if (count == 0) return true;
    Node& n = nodes[node];
    // Merge with the node's latest run if physically adjacent.
    for (int i = (int)extentCount - 1; i >= 0; i--) {
        Extent& e = extents[i];
        if (e.node != node) continue;
        if ((uint32_t)e.first + e.count == first && (uint32_t)e.count + count <= 0xFFFFu) {
            e.count += count;
            return true;
        }
        break;
    }
    if (extentCount >= kMaxExtents) {
        truncated = true;
        return false;
    }
    Extent& e = extents[extentCount++];
    e.first = first;
    e.count = count;
    e.node = node;
    e.next = kNoExtent;
    uint32_t fc = 0;
    for (uint16_t i = 0; i + 1 < extentCount; i++) {
        if (extents[i].node == node) fc += extents[i].count;
    }
    e.fileCluster = fc;
    if (n.extentCount == 0) n.firstCluster = first;
    n.extentCount++;
    return true;
}

void MediaLayout::finalizeExtents() {
    // Insertion sort by first cluster (n ≤ kMaxExtents, done once per media switch).
    for (uint16_t i = 1; i < extentCount; i++) {
        Extent key = extents[i];
        int j = (int)i - 1;
        while (j >= 0 && extents[j].first > key.first) {
            extents[j + 1] = extents[j];
            j--;
        }
        extents[j + 1] = key;
    }
    for (uint8_t n = 0; n < nodeCount; n++) {
        nodes[n].firstExtent = kNoExtent;
        int16_t prev = kNoExtent;
        uint32_t want = 0;
        // Link in file order (fileCluster ascending).
        for (uint16_t k = 0; k < nodes[n].extentCount; k++) {
            int16_t hit = kNoExtent;
            for (uint16_t i = 0; i < extentCount; i++) {
                if (extents[i].node == n && extents[i].fileCluster == want) {
                    hit = (int16_t)i;
                    break;
                }
            }
            if (hit == kNoExtent) break;
            extents[hit].next = kNoExtent;
            if (prev == kNoExtent) {
                nodes[n].firstExtent = hit;
                nodes[n].firstCluster = extents[hit].first;
            } else {
                extents[prev].next = hit;
            }
            want += extents[hit].count;
            prev = hit;
        }
    }
}

MediaLayout::Region MediaLayout::classify(uint32_t lba, int16_t* node, uint32_t* fileBlock) const {
    if (node) *node = -1;
    if (fileBlock) *fileBlock = 0;
    if (lba >= sectorCount) return Region::Beyond;
    if (lba == 0) return Region::Boot;
    if (lba < reserved) return Region::Reserved;
    if (lba < rootLba) return Region::Fat;
    if (lba < dataLba) return Region::Root;
    const uint32_t rel = lba - dataLba;
    const uint32_t cluster = rel / spc + 2;
    int16_t i = findExtent(cluster);
    if (i == kNoExtent) return Region::Free;
    const Extent& e = extents[i];
    if (node) *node = e.node;
    if (nodes[e.node].flags & kDir) return Region::Dir;
    if (fileBlock) *fileBlock = (e.fileCluster + (cluster - e.first)) * spc + rel % spc;
    return Region::File;
}

const char* MediaLayout::regionTag(Region r) {
    switch (r) {
        case Region::Boot: return "BOOT";
        case Region::Reserved: return "RSVD";
        case Region::Fat: return "FAT";
        case Region::Root: return "ROOT";
        case Region::Dir: return "DIR";
        case Region::File: return "DATA";
        case Region::Free: return "FREE";
        case Region::Beyond: return "OOR";
    }
    return "?";
}

const char* MediaLayout::regionKind(Region r) {
    switch (r) {
        case Region::Boot: return "boot";
        case Region::Reserved: return "rsvd";
        case Region::Fat: return "fat";
        case Region::Root: return "dir";
        case Region::Dir: return "dir";
        case Region::File: return "file";
        case Region::Free: return "free";
        case Region::Beyond: return "oor";
    }
    return "?";
}

void MediaLayout::nodePath(uint8_t idx, char* out, size_t n) const {
    if (!out || n == 0) return;
    out[0] = 0;
    if (idx >= nodeCount) return;
    uint8_t chain[8];
    uint8_t depth = 0;
    uint8_t cur = idx;
    while (cur != kNoParent && cur < nodeCount && depth < sizeof(chain)) {
        chain[depth++] = cur;
        cur = nodes[cur].parent;
    }
    size_t pos = 0;
    for (int d = (int)depth - 1; d >= 0; d--) {
        const char* nm = nodes[chain[d]].name;
        int w = snprintf(out + pos, n - pos, "%s%s", pos ? "/" : "", nm);
        if (w < 0 || (size_t)w >= n - pos) break;
        pos += (size_t)w;
    }
}

static void sfnToName(const uint8_t* e, char* out) {
    size_t p = 0;
    for (int i = 0; i < 8 && e[i] != ' '; i++) out[p++] = (char)e[i];
    if (e[8] != ' ') {
        out[p++] = '.';
        for (int i = 8; i < 11 && e[i] != ' '; i++) out[p++] = (char)e[i];
    }
    out[p] = 0;
}

bool MediaLayout::parseImage(const uint8_t* img, uint32_t size) {
    clear();
    if (!img || size < 512) return false;
    if (le16(img + 11) != 512) return false;
    spc = img[13] ? img[13] : 1;
    reserved = le16(img + 14);
    fats = img[16];
    rootEntries = le16(img + 17);
    uint32_t total = le16(img + 19);
    if (total == 0) total = le32(img + 32);
    fatSectors = le16(img + 22);
    sectorCount = total;
    if (img[38] == 0x29) {
        volumeId = le32(img + 39);
        memcpy(label, img + 43, 11);
        label[11] = 0;
        for (int i = 10; i >= 0 && label[i] == ' '; i--) label[i] = 0;
    }
    recomputeRegions();
    fatBits = (clusterCount < 4085) ? 12 : 16;
    if ((uint64_t)sectorCount * 512u > size) {
        // Image shorter than the volume (tail zero-filled by the reader).
        if ((uint64_t)dataLba * 512u > size) return false;
    }
    const uint8_t* fat = img + (uint32_t)reserved * 512u;
    auto fatRead = [&](uint32_t c) -> uint32_t {
        if (fatBits == 12) {
            uint32_t off = c + c / 2;
            uint16_t v = le16(fat + off);
            return (c & 1) ? (v >> 4) : (v & 0xFFF);
        }
        return le16(fat + c * 2);
    };
    const uint32_t eocMin = (fatBits == 12) ? 0xFF8u : 0xFFF8u;

    auto followChain = [&](uint8_t node, uint32_t first) -> uint32_t {
        uint32_t c = first;
        uint32_t steps = 0;
        uint32_t runStart = c;
        uint32_t runLen = 0;
        while (c >= 2 && c < clusterCount + 2 && steps <= clusterCount) {
            if (runLen && c == runStart + runLen) {
                runLen++;
            } else {
                if (runLen) addExtent(node, (uint16_t)runStart, (uint16_t)runLen);
                runStart = c;
                runLen = 1;
            }
            steps++;
            uint32_t nx = fatRead(c);
            if (nx >= eocMin || nx < 2) break;
            c = nx;
        }
        if (runLen) addExtent(node, (uint16_t)runStart, (uint16_t)runLen);
        return steps;
    };

    // Directory scan queue: kNoParent = root region, else dir node index.
    uint8_t queue[kMaxNodes + 1];
    uint8_t qh = 0, qt = 0;
    queue[qt++] = kNoParent;
    while (qh < qt) {
        const uint8_t dirNode = queue[qh++];
        // Collect the dir's byte ranges (root = fixed region; subdir = its extents).
        uint32_t ranges[16][2];
        uint8_t nr = 0;
        if (dirNode == kNoParent) {
            ranges[nr][0] = rootLba * 512u;
            ranges[nr][1] = rootSectors * 512u;
            nr++;
        } else {
            for (uint16_t i = 0; i < extentCount && nr < 16; i++) {
                if (extents[i].node != dirNode) continue;
                ranges[nr][0] = clusterLba(extents[i].first) * 512u;
                ranges[nr][1] = (uint32_t)extents[i].count * clusterBytes();
                nr++;
            }
        }
        bool end = false;
        for (uint8_t r = 0; r < nr && !end; r++) {
            for (uint32_t off = 0; off + 32 <= ranges[r][1]; off += 32) {
                const uint32_t abs = ranges[r][0] + off;
                if (abs + 32 > size) {
                    end = true;
                    break;
                }
                const uint8_t* e = img + abs;
                if (e[0] == 0x00) {
                    end = true;
                    break;
                }
                if (e[0] == 0xE5 || e[0] == '.') continue;
                const uint8_t attr = e[11];
                if (attr == 0x0F) continue;  // LFN
                if (attr & 0x08) {
                    if (dirNode == kNoParent && volumeId == 0) {
                        memcpy(label, e, 11);
                        label[11] = 0;
                        for (int i = 10; i >= 0 && label[i] == ' '; i--) label[i] = 0;
                    }
                    continue;
                }
                if (nodeCount >= kMaxNodes) {
                    truncated = true;
                    end = true;
                    break;
                }
                const uint8_t idx = nodeCount++;
                Node& n = nodes[idx];
                sfnToName(e, n.name);
                n.parent = dirNode;
                n.flags = (attr & 0x10) ? kDir : 0;
                const uint32_t first = le16(e + 26);
                const uint32_t steps = followChain(idx, first);
                n.size = (attr & 0x10) ? steps * clusterBytes() : le32(e + 28);
                if ((attr & 0x10) && qt <= kMaxNodes) queue[qt++] = idx;
            }
        }
    }
    finalizeExtents();
    return true;
}
