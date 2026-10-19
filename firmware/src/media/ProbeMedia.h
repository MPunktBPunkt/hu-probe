#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "media/MediaLayout.h"
#include "media/SynthFat.h"

class Session;

/**
 * Active medium of the probe (0.2.31): demo flash image or SynthFat variant.
 * Single source for MSC reads, LBA classification (B21) and the media metadata that
 * sessions carry (`session.media`, `ev:"media_layout"`).
 *
 * Switching is done by MscGadget::switchMedia() (hides the LUN first); never while an
 * offline capture is active.
 */
class ProbeMedia {
public:
    static ProbeMedia& instance();

    /** Load persisted variant (NVS "hpmedia"); falls back to demo_v1. */
    void begin();

    uint32_t sectorCount() const { return layout_.sectorCount; }
    int32_t read(uint32_t lba, uint32_t offset, uint8_t* out, uint32_t n);
    MediaLayout::Region classify(uint32_t lba, int16_t* node, uint32_t* fileBlock) const {
        return layout_.classify(lba, node, fileBlock);
    }
    const MediaLayout& layout() const { return layout_; }

    bool synthetic() const { return synthetic_; }
    const char* variantId() const { return id_; }
    const char* variantLabel() const { return label_; }
    uint32_t hash() const { return hash_; }
    /** Spec USB serial override ("" = keep gadget serial policy). */
    const char* usbSerial() const { return synthetic_ ? usbSerial_ : ""; }

    bool selectPreset(const char* id, String* err, bool persist = true);
    bool selectSpecJson(const String& json, String* err, bool persist = true);

    /**
     * In-place change of one SynthFat file without touching size, clusters or FAT:
     * seed (audio stamp + default title), ID3 texts, cover. Not persisted; the next
     * variant load restores the spec. -1 / mask bit clear = keep.
     */
    struct Mutation {
        int16_t node = -1;
        int16_t seed = -1;
        int8_t cover = -1;
        uint8_t tagMask = 0;
        MediaLayout::Tags tags;
    };
    static constexpr uint8_t kTagTitle = 1;
    static constexpr uint8_t kTagArtist = 2;
    static constexpr uint8_t kTagAlbum = 4;

    /** j["file"] (path, case-insensitive) or j["idx"] → file node, else -1 (dirs excluded). */
    int16_t resolveFile(JsonVariantConst j) const;

    /** {"file":"DIR/A.MP3"|"idx":n, "seed","title","artist","album","cover"} → m. */
    bool parseMutation(JsonVariantConst j, Mutation& m, String* err) const;
    bool applyMutation(const Mutation& m, String* err);
    uint32_t mutationGen() const { return mutationGen_; }
    /** Short "n3 seed,title,cover" for markers. */
    static void describeMutation(const Mutation& m, char* out, size_t n);

    void toJson(JsonObject o, bool withFiles) const;
    static void presetsJson(JsonArray arr);
    static bool hasPreset(const char* id);
    /** One `media_layout` event per node (path, size, LBA extents) into the session. */
    void publishLayout(Session* s) const;

private:
    ProbeMedia() = default;
    bool loadDemo();
    bool loadSpec(JsonVariantConst spec, const char* id, const char* label, const String& canon,
                  String* err);
    void persist(const char* id, const String* json) const;

    MediaLayout layout_;
    SynthFat synth_;
    char usbSerial_[sizeof(SynthFat::Spec::usbSerial)] = {0};
    bool synthetic_ = false;
    char id_[24] = "demo_v1";
    char label_[64] = {0};
    uint32_t hash_ = 0;
    uint32_t mutationGen_ = 0;
};
