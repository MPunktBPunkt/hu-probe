#include "media/ProbeMedia.h"
#include "media/CoverJpegs.h"
#include "media/MediaPresets.h"
#include "media/Mp3Gen.h"
#include "msc/DemoFatImage.h"
#include "msc/MscBotTap.h"
#include "core/ProbeClock.h"
#include "net/EventBus.h"
#include "session/Session.h"
#include <Preferences.h>
#include <cstring>
#include <new>

namespace {

uint32_t fnv1a(const uint8_t* p, size_t n, uint32_t h = 2166136261u) {
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

void hex32(uint32_t v, char* out) { snprintf(out, 11, "0x%08x", (unsigned)v); }

}  // namespace

ProbeMedia& ProbeMedia::instance() {
    static ProbeMedia m;
    return m;
}

bool ProbeMedia::loadDemo() {
    if (!layout_.parseImage(DEMO_FAT_IMAGE, DEMO_FAT_SIZE)) {
        // Keep reads working even if the BPB is odd: whole LUN as one image.
        layout_.clear();
        layout_.sectorCount = DEMO_FAT_SECTOR_COUNT;
    }
    if (layout_.sectorCount == 0 || layout_.sectorCount > DEMO_FAT_SECTOR_COUNT) {
        layout_.sectorCount = DEMO_FAT_SECTOR_COUNT;
    }
    synthetic_ = false;
    snprintf(id_, sizeof(id_), "demo_v1");
    const MediaPreset* p = findMediaPreset("demo_v1");
    snprintf(label_, sizeof(label_), "%s", p ? p->label : "demo");
    hash_ = fnv1a(DEMO_FAT_IMAGE, DEMO_FAT_SIZE);
    return true;
}

bool ProbeMedia::loadSpec(JsonVariantConst j, const char* id, const char* label,
                          const String& canon, String* err) {
    SynthFat::Spec* spec = new (std::nothrow) SynthFat::Spec();
    MediaLayout* tmp = new (std::nothrow) MediaLayout();
    if (!spec || !tmp) {
        delete spec;
        delete tmp;
        if (err) *err = "oom";
        return false;
    }
    bool ok = SynthFat::parseSpec(j, *spec, err) && synth_.build(*spec, *tmp, err);
    if (ok) {
        if (id && id[0]) snprintf(spec->id, sizeof(spec->id), "%s", id);
        layout_ = *tmp;
        snprintf(usbSerial_, sizeof(usbSerial_), "%s", spec->usbSerial);
        synthetic_ = true;
        snprintf(id_, sizeof(id_), "%s", spec->id);
        snprintf(label_, sizeof(label_), "%s", (label && label[0]) ? label : spec->label);
        hash_ = fnv1a((const uint8_t*)canon.c_str(), canon.length());
    }
    delete spec;
    delete tmp;
    return ok;
}

void ProbeMedia::persist(const char* id, const String* json) const {
    Preferences prefs;
    if (!prefs.begin("hpmedia", false)) return;
    prefs.putString("id", id);
    if (json) {
        prefs.putString("spec", *json);
    } else {
        prefs.remove("spec");
    }
    prefs.end();
}

bool ProbeMedia::selectPreset(const char* id, String* err, bool doPersist) {
    const MediaPreset* p = findMediaPreset(id);
    if (!p) {
        if (err) *err = "unknown_preset";
        return false;
    }
    if (!p->json) {
        loadDemo();
        if (doPersist) persist(p->id, nullptr);
        return true;
    }
    JsonDocument* doc = new (std::nothrow) JsonDocument();
    if (!doc) {
        if (err) *err = "oom_json";
        return false;
    }
    if (deserializeJson(*doc, p->json)) {
        delete doc;
        if (err) *err = "preset_json";
        return false;
    }
    String canon;
    serializeJson(*doc, canon);
    const bool okLoad = loadSpec(doc->as<JsonVariantConst>(), p->id, p->label, canon, err);
    delete doc;
    if (!okLoad) return false;
    if (doPersist) persist(p->id, nullptr);
    return true;
}

bool ProbeMedia::selectSpecJson(const String& json, String* err, bool doPersist) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        if (err) *err = "spec_json";
        return false;
    }
    String canon;
    serializeJson(doc, canon);
    const char* id = doc["id"] | "custom";
    if (!loadSpec(doc.as<JsonVariantConst>(), id, doc["label"] | "", canon, err)) return false;
    if (doPersist) {
        if (canon.length() > 3800) {
            if (err) *err = "applied_not_persisted_too_long";
        } else {
            persist("custom", &canon);
        }
    }
    return true;
}

void ProbeMedia::begin() {
    Preferences prefs;
    String id = "demo_v1";
    String spec;
    if (prefs.begin("hpmedia", true)) {
        id = prefs.getString("id", "demo_v1");
        if (id == "custom") spec = prefs.getString("spec", "");
        prefs.end();
    }
    String err;
    bool ok = false;
    if (id == "custom" && spec.length()) {
        ok = selectSpecJson(spec, &err, false);
    } else {
        ok = selectPreset(id.c_str(), &err, false);
    }
    if (!ok) {
        Serial.printf("[MEDIA] variant %s failed (%s) — demo_v1\n", id.c_str(), err.c_str());
        loadDemo();
    }
    Serial.printf("[MEDIA] %s sectors=%u nodes=%u fat%u spc=%u data_lba=%u\n", id_,
                  (unsigned)layout_.sectorCount, (unsigned)layout_.nodeCount,
                  (unsigned)layout_.fatBits, (unsigned)layout_.spc, (unsigned)layout_.dataLba);
}

int32_t ProbeMedia::read(uint32_t lba, uint32_t offset, uint8_t* out, uint32_t n) {
    if (!synthetic_) {
        const uint32_t pos = lba * DEMO_FAT_SECTOR_SIZE + offset;
        if (pos >= DEMO_FAT_SIZE) {
            memset(out, 0, n);
        } else {
            uint32_t take = DEMO_FAT_SIZE - pos;
            if (take > n) take = n;
            memcpy(out, DEMO_FAT_IMAGE + pos, take);
            if (take < n) memset(out + take, 0, n - take);
        }
        return (int32_t)n;
    }
    uint32_t done = 0;
    uint32_t cur = lba + offset / 512u;
    uint32_t inSec = offset % 512u;
    while (done < n) {
        const uint32_t take = (512u - inSec < n - done) ? (512u - inSec) : (n - done);
        if (inSec == 0 && take == 512u) {
            synth_.sector(layout_, cur, out + done);
        } else {
            uint8_t sec[512];
            synth_.sector(layout_, cur, sec);
            memcpy(out + done, sec + inSec, take);
        }
        done += take;
        inSec = 0;
        cur++;
    }
    return (int32_t)n;
}

int16_t ProbeMedia::resolveFile(JsonVariantConst j) const {
    int16_t node = -1;
    if (j["file"].is<const char*>()) {
        const char* want = j["file"].as<const char*>();
        while (*want == '/') want++;
        char path[64];
        for (uint8_t i = 0; i < layout_.nodeCount; i++) {
            layout_.nodePath(i, path, sizeof(path));
            if (strcasecmp(path, want) == 0) {
                node = i;
                break;
            }
        }
    } else if (j["idx"].is<int>()) {
        const int i = j["idx"].as<int>();
        if (i >= 0 && i < layout_.nodeCount) node = (int16_t)i;
    }
    if (node >= 0 && (layout_.nodes[node].flags & MediaLayout::kDir)) node = -1;
    return node;
}

bool ProbeMedia::parseMutation(JsonVariantConst j, Mutation& m, String* err) const {
    m = Mutation();
    if (!synthetic_) {
        if (err) *err = "needs_synthfat";
        return false;
    }
    m.node = resolveFile(j);
    if (m.node < 0) {
        if (err) *err = "unknown_file";
        return false;
    }
    const MediaLayout::Node& n = layout_.nodes[m.node];
    if (!j["seed"].isNull()) {
        const int s = j["seed"] | -1;
        if (s < 0 || s > 255) {
            if (err) *err = "seed_0_255";
            return false;
        }
        m.seed = (int16_t)s;
    }
    if (!j["cover"].isNull()) {
        const int c = j["cover"] | -1;
        if (c < 0 || c > CoverJpegs::kCount) {
            if (err) *err = "cover_0_4";
            return false;
        }
        const CoverJpegs::Cover* cv = CoverJpegs::get((uint8_t)c);
        if (!n.apicBytes || (cv && cv->len > n.apicBytes)) {
            if (err) *err = "no_cover_slot";
            return false;
        }
        m.cover = (int8_t)c;
    }
    struct {
        const char* key;
        char* dst;
        uint8_t bit;
    } const fields[] = {{"title", m.tags.title, kTagTitle},
                        {"artist", m.tags.artist, kTagArtist},
                        {"album", m.tags.album, kTagAlbum}};
    for (const auto& f : fields) {
        if (j[f.key].isNull()) continue;
        if (!SynthFat::setTag(f.dst, j[f.key], err)) return false;
        m.tagMask |= f.bit;
    }
    if (m.tagMask && (n.flags & MediaLayout::kNoId3v2) && !(n.flags & MediaLayout::kId3v1)) {
        if (err) *err = "file_has_no_tag";
        return false;
    }
    if (m.seed < 0 && m.cover < 0 && !m.tagMask) {
        if (err) *err = "nothing_to_change";
        return false;
    }
    return true;
}

bool ProbeMedia::applyMutation(const Mutation& m, String* err) {
    if (!synthetic_ || m.node < 0 || m.node >= layout_.nodeCount) {
        if (err) *err = "unknown_file";
        return false;
    }
    // Reads run concurrently on the USB task; a sector generated mid-update may mix old
    // and new bytes. Size/clusters never change, so the FS stays consistent.
    MediaLayout::Node& n = layout_.nodes[m.node];
    MediaLayout::Tags& t = layout_.tags[m.node];
    if (m.seed >= 0) n.seed = (uint8_t)m.seed;
    if (m.cover >= 0) n.cover = (uint8_t)m.cover;
    if (m.tagMask & kTagTitle) memcpy(t.title, m.tags.title, sizeof(t.title));
    if (m.tagMask & kTagArtist) memcpy(t.artist, m.tags.artist, sizeof(t.artist));
    if (m.tagMask & kTagAlbum) memcpy(t.album, m.tags.album, sizeof(t.album));
    mutationGen_++;
    return true;
}

void ProbeMedia::describeMutation(const Mutation& m, char* out, size_t n) {
    snprintf(out, n, "n%d%s%s%s%s%s", (int)m.node, m.seed >= 0 ? " seed" : "",
             (m.tagMask & kTagTitle) ? " title" : "", (m.tagMask & kTagArtist) ? " artist" : "",
             (m.tagMask & kTagAlbum) ? " album" : "", m.cover >= 0 ? " cover" : "");
}

void ProbeMedia::toJson(JsonObject o, bool withFiles) const {
    char h[12];
    o["variant"] = id_;
    o["label"] = label_;
    o["synthetic"] = synthetic_;
    hex32(hash_, h);
    o["hash"] = h;
    o["fs"] = layout_.fatBits == 12 ? "FAT12" : "FAT16";
    o["sector_count"] = layout_.sectorCount;
    o["spc"] = layout_.spc;
    o["reserved"] = layout_.reserved;
    o["fat_sectors"] = layout_.fatSectors;
    o["root_lba"] = layout_.rootLba;
    o["data_lba"] = layout_.dataLba;
    o["cluster_count"] = layout_.clusterCount;
    hex32(layout_.volumeId, h);
    o["volume_id"] = h;
    o["fs_label"] = layout_.label;
    o["usb_serial_override"] = usbSerial();
    o["nodes"] = layout_.nodeCount;
    o["truncated"] = layout_.truncated;
    o["mutation_gen"] = mutationGen_;
    o["ua_pending"] = MscBotTap::unitAttentionPending();
    o["ua_sent"] = MscBotTap::unitAttentionSent();
    if (!withFiles) return;
    JsonArray files = o["files"].to<JsonArray>();
    char path[64];
    for (uint8_t i = 0; i < layout_.nodeCount; i++) {
        const MediaLayout::Node& n = layout_.nodes[i];
        JsonObject f = files.add<JsonObject>();
        layout_.nodePath(i, path, sizeof(path));
        f["idx"] = i;
        f["path"] = path;
        f["dir"] = (n.flags & MediaLayout::kDir) != 0;
        f["size"] = n.size;
        if (n.firstExtent != MediaLayout::kNoExtent) {
            f["first_lba"] = layout_.clusterLba(layout_.extents[n.firstExtent].first);
        }
        f["extents"] = n.extentCount;
        if (n.kbps) f["kbps"] = n.kbps;
        if (n.durMs) f["dur_ms"] = n.durMs;
        if (n.seed) f["seed"] = n.seed;
        if (!synthetic_ || (n.flags & MediaLayout::kDir)) continue;
        if (n.apicBytes) {
            f["apic_bytes"] = n.apicBytes;
            f["cover"] = n.cover;
        }
        if (!(n.flags & MediaLayout::kNoId3v2) || (n.flags & MediaLayout::kId3v1)) {
            Mp3Gen::Texts x;
            Mp3Gen::resolveTexts(n, &layout_.tags[i], x);
            f["title"] = x.title;
            f["artist"] = x.artist;
            f["album"] = x.album;
        }
    }
}

void ProbeMedia::presetsJson(JsonArray arr) {
    for (size_t i = 0; i < kMediaPresetCount; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = kMediaPresets[i].id;
        o["label"] = kMediaPresets[i].label;
        o["synthetic"] = kMediaPresets[i].json != nullptr;
    }
}

bool ProbeMedia::hasPreset(const char* id) { return findMediaPreset(id) != nullptr; }

void ProbeMedia::publishLayout(Session* s) const {
    if (!s || !s->active()) return;
    char path[64];
    for (uint8_t i = 0; i < layout_.nodeCount; i++) {
        const MediaLayout::Node& n = layout_.nodes[i];
        JsonDocument doc;
        int64_t t = ProbeClock::publishUs(ProbeClock::eventNowUs());
        doc["v"] = 2;
        doc["sid"] = s->id();
        doc["seq"] = s->nextSeq();
        doc["t_us"] = (long long)t;
        doc["t_ms"] = ProbeClock::nowMsCompat(t);
        doc["ev"] = "media_layout";
        doc["variant"] = id_;
        doc["node"] = i;
        layout_.nodePath(i, path, sizeof(path));
        doc["path"] = path;
        doc["dir"] = (n.flags & MediaLayout::kDir) != 0;
        doc["size"] = n.size;
        if (n.kbps) doc["kbps"] = n.kbps;
        if (n.durMs) doc["dur_ms"] = n.durMs;
        if (n.seed) doc["seed"] = n.seed;
        if (n.flags) doc["flags"] = n.flags;
        if (n.apicBytes) {
            doc["apic_bytes"] = n.apicBytes;
            doc["cover"] = n.cover;
        }
        if (synthetic_ && !(n.flags & MediaLayout::kDir)) {
            Mp3Gen::Texts x;
            Mp3Gen::resolveTexts(n, &layout_.tags[i], x);
            doc["title"] = x.title;
        }
        // [first_lba, sectors] in file order
        JsonArray ex = doc["extents"].to<JsonArray>();
        int16_t e = n.firstExtent;
        uint8_t emitted = 0;
        while (e != MediaLayout::kNoExtent && emitted < 32) {
            const MediaLayout::Extent& x = layout_.extents[e];
            JsonArray pair = ex.add<JsonArray>();
            pair.add(layout_.clusterLba(x.first));
            pair.add((uint32_t)x.count * layout_.spc);
            e = x.next;
            emitted++;
        }
        if (e != MediaLayout::kNoExtent) doc["extents_truncated"] = true;
        EventBus::instance().publish(doc);
    }
}
