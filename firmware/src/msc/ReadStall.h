#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class Session;

/**
 * READ10 brake (BUSY pacing) for SEQ-STALL (0.2.31).
 *
 * A matching READ10 command is held for delayMs before its first data byte: the MSC
 * read callback returns 0 (TinyUSB retries the same chunk) with a 2 ms yield per retry,
 * never < 0 (that would STALL the endpoint). The delay is per command, keyed on the
 * CBW tag — busy retries re-enter with identical lba/offset and must not restart the
 * timer (PiDrive 0.4.47 bug). BOT is queue depth 1, so no TUR arrives while a READ10
 * is held; the HU's patience per command is exactly what the ladder measures.
 *
 * A brake that was active during an offline capture switches off when the capture
 * ends, so finalize/upload never run against a held command.
 *
 * Filter: regions (meta = boot/reserved/FAT/root/dir, head = first headBlocks of a
 * file, body = rest of the file data), optional single file, count of commands,
 * start delay and window. Every held command becomes a `read_stall` marker
 * (held/abandoned, wait_ms); `abandoned` = host sent another CBW (or reset) before
 * the delay ran out.
 */
class ReadStall {
public:
    static constexpr uint8_t kMeta = 1;
    static constexpr uint8_t kHead = 2;
    static constexpr uint8_t kBody = 4;
    static constexpr uint32_t kMaxDelayMs = 120000;

    struct Config {
        uint8_t regions = kBody;
        int16_t node = -1;  // -1 = any file
        uint32_t delayMs = 1000;
        uint16_t count = 1;  // commands to hold, 0 = unlimited (until window/disable)
        uint16_t headBlocks = 16;
        uint32_t armDelayMs = 0;
        uint32_t windowMs = 0;  // 0 = no time limit
        /**
         * > 0: per-command delay = CBW bytes at the file's bitrate × paceFactor (a live
         * stream with zero lead, PiDrive without ring); delayMs is the fallback for
         * meta reads / files without bitrate. 0 = fixed delayMs.
         */
        float paceFactor = 0.0f;
    };

    static ReadStall& instance();

    /**
     * {regions:["meta","head","body"], file|idx, delay_ms, count, head_blocks,
     *  arm_delay_s, window_s, pace:"fixed"|"realtime", pace_factor}.
     */
    bool configure(JsonVariantConst j, String* err);
    void disable(const char* reason);
    bool enabled() const { return enabled_; }

    /** USB task, READ10 data callback: true = answer this chunk with BUSY. */
    bool hold(uint32_t lba);
    /** USB bus reset / unplug / media switch: a held command is gone. */
    void onUsbReset();
    /** Arduino loop: start/window timers, host-abandon detection, markers. */
    void poll(Session* session);

    void toJson(JsonObject o) const;
    static void regionsToJson(uint8_t regions, JsonArray a);

private:
    struct Done {
        int64_t t0Us;
        uint32_t lba;
        uint32_t waitMs;
        uint32_t targetMs;
        uint32_t bytes;
        int16_t node;
        uint8_t region;  // kMeta / kHead / kBody
        bool abandoned;
    };

    ReadStall() = default;
    uint8_t classify(uint32_t lba, int16_t* node) const;
    /** Caller holds mux_. */
    void finishLocked(bool abandoned);
    void emit(const Done& d, Session* session);

    Config cfg_;
    volatile bool enabled_ = false;  // configured, maybe still waiting for armDelay
    volatile bool active_ = false;   // holding new commands allowed now
    bool pendingStart_ = false;      // enabled, armDelay not yet elapsed
    bool offMarkPending_ = false;
    bool capturedWhileEnabled_ = false;  // ends with the offline capture it ran in
    uint32_t enabledAtMs_ = 0;
    uint32_t activeSinceMs_ = 0;
    const char* lastReason_ = "";

    // USB-task state (guarded by mux_)
    bool stalling_ = false;
    uint32_t stallTag_ = 0;
    uint32_t decidedTag_ = 0xFFFFFFFFu;
    uint32_t t0Ms_ = 0;
    uint32_t targetMs_ = 0;
    uint32_t bytes_ = 0;
    int64_t t0Us_ = 0;
    uint32_t lba_ = 0;
    int16_t node_ = -1;
    uint8_t region_ = 0;

    uint32_t held_ = 0;
    uint32_t abandoned_ = 0;
    uint32_t retries_ = 0;
    uint32_t maxWaitMs_ = 0;
    uint32_t lastWaitMs_ = 0;

    static constexpr uint8_t kDoneRing = 16;
    Done done_[kDoneRing];
    uint8_t doneHead_ = 0;
    uint8_t doneCount_ = 0;
    uint32_t doneDropped_ = 0;
    uint16_t markersEmitted_ = 0;
};
