#pragma once

/**
 * @file ProbeMedia.h
 * @brief Singleton owning the active USB medium (demo image or SynthFat variant).
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "media/MediaLayout.h"
#include "media/SynthFat.h"

class Session;

/**
 * @brief Active medium of the probe (0.2.31): demo flash image or SynthFat variant.
 *
 * Single source for MSC reads, LBA classification (B21) and the media metadata that
 * sessions carry (`session.media`, `ev:"media_layout"`).
 *
 * Switching is done by MscGadget::switchMedia() (hides the LUN first); never while an
 * offline capture is active.
 */
class ProbeMedia {
public:
    /**
     * @brief Return the process-wide instance (function-local static).
     *
     * @return The single ProbeMedia object.
     */
    static ProbeMedia& instance();

    /**
     * @brief Load persisted variant (NVS "hpmedia"); falls back to demo_v1.
     *
     * Restores a preset by id or, for id "custom", the stored spec JSON. Logs the result to
     * Serial.
     */
    void begin();

    /** @brief Volume size in 512-byte sectors of the active medium. */
    uint32_t sectorCount() const { return layout_.sectorCount; }

    /**
     * @brief Read bytes of the active medium (MSC read callback).
     *
     * Demo image: copied from flash, zero-filled past the image end. SynthFat: generated
     * sector by sector.
     *
     * @param lba       First sector (512 bytes).
     * @param offset    Byte offset from the start of lba (may exceed 512).
     * @param[out] out  Destination buffer of n bytes.
     * @param n         Number of bytes to read.
     * @return Always n (bytes delivered).
     * @note Runs on the USB task, concurrently with applyMutation().
     */
    int32_t read(uint32_t lba, uint32_t offset, uint8_t* out, uint32_t n);

    /**
     * @brief Classify an LBA of the active medium (B21), see MediaLayout::classify().
     *
     * @param lba             Sector address to classify.
     * @param[out] node       Owning node index for Dir/File, else -1; may be nullptr.
     * @param[out] fileBlock  512-byte block index within the file, else 0; may be nullptr.
     * @return Region of the LBA.
     */
    MediaLayout::Region classify(uint32_t lba, int16_t* node, uint32_t* fileBlock) const {
        return layout_.classify(lba, node, fileBlock);
    }

    /** @brief FAT geometry and node/extent map of the active medium. */
    const MediaLayout& layout() const { return layout_; }

    /** @brief true if a SynthFat variant is active, false for the demo flash image. */
    bool synthetic() const { return synthetic_; }

    /** @brief Active variant id (preset id, spec "id" or "custom"; default "demo_v1"). */
    const char* variantId() const { return id_; }

    /** @brief Human-readable label of the active variant. */
    const char* variantLabel() const { return label_; }

    /** @brief FNV-1a hash of the demo image or of the canonical spec JSON. */
    uint32_t hash() const { return hash_; }

    /** @brief Spec USB serial override ("" = keep gadget serial policy). */
    const char* usbSerial() const { return synthetic_ ? usbSerial_ : ""; }

    /**
     * @brief Activate a built-in preset from MediaPresets.h.
     *
     * Presets without JSON load the demo image; all others are built by SynthFat.
     *
     * @param id        Preset id, e.g. "cache_a".
     * @param[out] err  Error code on failure ("unknown_preset", "oom_json", "preset_json" or a
     *                  SynthFat error); may be nullptr.
     * @param persist   Store the preset id in NVS "hpmedia".
     * @return true if the preset is active.
     * @warning Call via MscGadget::switchMedia(), which hides the LUN first.
     */
    bool selectPreset(const char* id, String* err, bool persist = true);

    /**
     * @brief Activate a custom SynthFat spec given as JSON.
     *
     * The variant id is the spec's "id" (default "custom"). When persisting, the canonical
     * JSON is stored under id "custom" only if it is at most 3800 chars; otherwise the spec is
     * applied, not persisted, and err is set to "applied_not_persisted_too_long" while the
     * function still returns true.
     *
     * @param json      SynthFat spec JSON.
     * @param[out] err  Error code ("spec_json" or a SynthFat error); may be nullptr.
     * @param persist   Store the spec in NVS "hpmedia".
     * @return true if the spec was parsed and built and is active.
     * @warning Call via MscGadget::switchMedia(), which hides the LUN first.
     */
    bool selectSpecJson(const String& json, String* err, bool persist = true);

    /**
     * @brief In-place change of one SynthFat file without touching size, clusters or FAT.
     *
     * Changes seed (audio stamp + default title), ID3 texts, cover. Not persisted; the next
     * variant load restores the spec. -1 / mask bit clear = keep.
     */
    struct Mutation {
        int16_t node = -1;    ///< target file node index
        int16_t seed = -1;    ///< new seed 0..255, -1 = keep
        int8_t cover = -1;    ///< new cover 0..CoverJpegs::kCount, -1 = keep
        uint8_t tagMask = 0;  ///< kTagTitle/kTagArtist/kTagAlbum bits of tags to apply
        MediaLayout::Tags tags;  ///< new ID3 texts (only fields selected by tagMask)
    };
    static constexpr uint8_t kTagTitle = 1;
    static constexpr uint8_t kTagArtist = 2;
    static constexpr uint8_t kTagAlbum = 4;

    /**
     * @brief Resolve a file node from a JSON request.
     *
     * j["file"] (path, case-insensitive, leading '/' ignored) or j["idx"] → file node,
     * else -1 (dirs excluded).
     *
     * @param j  Request object with "file" or "idx".
     * @return File node index, or -1 if not found or a directory.
     */
    int16_t resolveFile(JsonVariantConst j) const;

    /**
     * @brief Parse and validate a mutation request.
     *
     * {"file":"DIR/A.MP3"|"idx":n, "seed","title","artist","album","cover"} → m.
     * A cover must fit the file's APIC slot; tags need an ID3v2 or ID3v1 tag in the file.
     *
     * @param j         Request object.
     * @param[out] m    Parsed mutation (reset first).
     * @param[out] err  Error code ("needs_synthfat", "unknown_file", "seed_0_255", "cover_0_4",
     *                  "no_cover_slot", "file_has_no_tag", "nothing_to_change" or a tag error);
     *                  may be nullptr.
     * @return true if m holds at least one valid change.
     */
    bool parseMutation(JsonVariantConst j, Mutation& m, String* err) const;

    /**
     * @brief Apply a parsed mutation to the live layout and bump mutationGen().
     *
     * @param m         Mutation from parseMutation().
     * @param[out] err  "unknown_file" if no SynthFat medium is active or the node is invalid;
     *                  may be nullptr.
     * @return true if applied.
     * @note Reads run concurrently on the USB task; a sector generated mid-update may mix old
     *       and new bytes. Size/clusters never change, so the FS stays consistent.
     */
    bool applyMutation(const Mutation& m, String* err);

    /** @brief Number of mutations applied since boot (not reset on variant switch). */
    uint32_t mutationGen() const { return mutationGen_; }

    /**
     * @brief Format a short description such as "n3 seed title cover" for markers.
     *
     * @param m         Mutation to describe.
     * @param[out] out  Destination buffer.
     * @param n         Size of out in bytes.
     */
    static void describeMutation(const Mutation& m, char* out, size_t n);

    /**
     * @brief Serialize variant, hash, FAT geometry and unit-attention state into o.
     *
     * @param o          Target JSON object.
     * @param withFiles  Also add a "files" array (path, size, first LBA, extents, tags).
     */
    void toJson(JsonObject o, bool withFiles) const;

    /**
     * @brief Append all built-in presets (id, label, synthetic) to arr.
     *
     * @param arr  Target JSON array.
     */
    static void presetsJson(JsonArray arr);

    /**
     * @brief Check whether a built-in preset with this id exists.
     *
     * @param id  Preset id; nullptr allowed.
     * @return true if findMediaPreset() knows the id.
     */
    static bool hasPreset(const char* id);

    /**
     * @brief Publish one `media_layout` event per node (path, size, LBA extents) into the session.
     *
     * At most 32 extents per node are listed; "extents_truncated" marks the rest.
     *
     * @param s  Target session; nothing is published if null or inactive.
     */
    void publishLayout(Session* s) const;

private:
    /** @brief Construct the singleton; use instance(). */
    ProbeMedia() = default;

    /**
     * @brief Activate the demo flash image (FAT12).
     *
     * If the BPB cannot be parsed, the whole LUN is presented as one unclassified image.
     *
     * @return Always true.
     */
    bool loadDemo();

    /**
     * @brief Build a SynthFat layout from a parsed spec and make it active.
     *
     * Builds into a heap-allocated temporary so the active layout only changes on success.
     *
     * @param spec      Parsed spec JSON.
     * @param id        Variant id overriding the spec id; nullptr/empty = keep spec id.
     * @param label     Label overriding the spec label; nullptr/empty = keep spec label.
     * @param canon     Canonical JSON text used for the variant hash.
     * @param[out] err  Error code ("oom" or a SynthFat error); may be nullptr.
     * @return true if the variant is active.
     */
    bool loadSpec(JsonVariantConst spec, const char* id, const char* label, const String& canon,
                  String* err);

    /**
     * @brief Store the variant id (and optional spec JSON) in NVS namespace "hpmedia".
     *
     * @param id    Variant id to store.
     * @param json  Spec JSON to store; nullptr removes a stored spec.
     */
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
