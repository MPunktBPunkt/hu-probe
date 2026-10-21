#pragma once

/**
 * @file ReadStall.h
 * @brief READ10 brake that holds selected host reads with BUSY retries (SEQ-STALL).
 */

#include <Arduino.h>
#include <ArduinoJson.h>

class Session;

/**
 * @brief READ10 brake (BUSY pacing) for SEQ-STALL (0.2.31).
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
    /** @brief Region bit: boot sector, reserved sectors, FAT, root and directory clusters. */
    static constexpr uint8_t kMeta = 1;

    /** @brief Region bit: first Config::headBlocks blocks of a file. */
    static constexpr uint8_t kHead = 2;

    /** @brief Region bit: file data after the head blocks. */
    static constexpr uint8_t kBody = 4;

    /** @brief Upper limit for the per-command hold time in ms (120 s). */
    static constexpr uint32_t kMaxDelayMs = 120000;

    /** @brief Brake filter and timing, set by configure(). */
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

    /** @brief Process-wide brake instance (function-local static). */
    static ReadStall& instance();

    /**
     * @brief Validate a JSON config and arm the brake (replacing any previous configuration).
     *
     * {regions:["meta","head","body"], file|idx, delay_ms, count, head_blocks,
     *  arm_delay_s, window_s, pace:"fixed"|"realtime", pace_factor}.
     * "all" is accepted as region. Limits: delay_ms 1..120000, pace_factor (0, 10],
     * arm_delay_s 0..600, window_s 0..3600; a file filter cannot be combined with meta.
     * Resets counters and never holds the command already in flight. Holding starts after
     * arm_delay_s in poll().
     *
     * @param j         Config object.
     * @param[out] err  Error code on failure (e.g. "unknown_file", "delay_ms_1_120000"); may be nullptr.
     * @return true if armed, false if the config was rejected (previous state unchanged).
     * @note Arduino loop task (web/API handler).
     */
    bool configure(JsonVariantConst j, String* err);

    /**
     * @brief Switch the brake off; a held command is released on its next retry.
     *
     * Queues a stall_off marker for the next poll(). No-op if already off.
     *
     * @param reason  Static reason string reported as last_off (e.g. "window", "count"); nullptr = "off".
     */
    void disable(const char* reason);

    /** @brief True while configured (armed or active), until disable(). */
    bool enabled() const { return enabled_; }

    /**
     * @brief Decide whether to answer this READ10 chunk with BUSY.
     *
     * USB task, READ10 data callback: true = answer this chunk with BUSY.
     * Each CBW tag is decided once; a held command keeps returning true until its target
     * time elapsed. Blocks the USB task for 2 ms per true result.
     *
     * @param lba  Logical block address of the chunk.
     * @return true = return 0 from the read callback (retry), false = serve the data.
     */
    bool hold(uint32_t lba);

    /**
     * @brief Finish a held command as abandoned and forget the decided CBW tag.
     *
     * USB bus reset / unplug / media switch: a held command is gone.
     */
    void onUsbReset();

    /**
     * @brief Run start/window timers, host-abandon detection and marker output.
     *
     * Arduino loop: start/window timers, host-abandon detection, markers.
     * Also disables the brake when the offline capture it ran in ends, and after count commands.
     *
     * @param session  Session for markers (offline capture takes precedence); may be nullptr.
     */
    void poll(Session* session);

    /**
     * @brief Write state, configuration and counters into a JSON object.
     * @param o  Target JSON object.
     */
    void toJson(JsonObject o) const;

    /**
     * @brief Append the names of the set region bits ("meta", "head", "body").
     * @param regions  Bitmask of kMeta / kHead / kBody.
     * @param a        Target JSON array.
     */
    static void regionsToJson(uint8_t regions, JsonArray a);

private:
    /** @brief Result of one held command, queued in the USB task and emitted by poll(). */
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

    /** @brief Private constructor; use instance(). */
    ReadStall() = default;

    /**
     * @brief Classify an LBA into a region bit using the active medium's layout.
     * @param lba        Logical block address.
     * @param[out] node  File node index, or -1 if lba is not file data.
     * @return kMeta, kHead or kBody, or 0 for unclassified areas.
     */
    uint8_t classify(uint32_t lba, int16_t* node) const;

    /**
     * @brief End the held command, update counters and queue a Done record.
     *
     * Caller holds mux_. Deactivates the brake once count commands were held.
     *
     * @param abandoned  true if the host moved on (new CBW or reset) before the target time.
     */
    void finishLocked(bool abandoned);

    /**
     * @brief Emit a read_stall / stall_abandon marker for a finished command (max 64 per config).
     * @param d        Finished command.
     * @param session  Session for the marker; may be nullptr.
     */
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
