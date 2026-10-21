#pragma once

/**
 * @file SequenceRunner.h
 * @brief State machine that runs a predefined ExperimentSequence as a series of offline captures.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "capture/ExperimentProfiles.h"

class Session;
class MscGadget;
class ReadInstrument;
class TcpStreamer;

/**
 * @brief Runs an ExperimentSequence step by step (0.2.31, R1).
 *
 *   Prepare     switch media (LUN hidden while rebuilding), apply serial mode
 *   WaitDriver  LED prompt; BOOT short or POST /api/sequence/next = arm,
 *               BOOT long = skip step (auto_arm: arm after settle without BOOT)
 *   Running     OfflineCapture owns WiFi/USB until upload is done
 *   Evaluate    overflow -> rerun the step as two half-length parts (once)
 *
 * Scenario per run: "<seq>_<step>r<rep>[p<part>]" so sessions sort per step.
 */
class SequenceRunner {
public:
    /** @brief Runner phase; Prepare, WaitDriver and Running count as active(). */
    enum class State : uint8_t { Idle = 0, Prepare, WaitDriver, Running, Done, Failed };

    /**
     * @brief Return the process-wide singleton (created on first call).
     * @return Reference to the single SequenceRunner instance.
     */
    static SequenceRunner& instance();

    /**
     * @brief Store the collaborators handed to OfflineCapture when a step is armed.
     * @param session  Live session passed to OfflineCapture::arm().
     * @param msc      MSC gadget used for media switching and serial pinning.
     * @param inst     READ instrumentation passed to OfflineCapture::arm().
     * @param tcp      TCP streamer passed to OfflineCapture::arm().
     */
    void begin(Session* session, MscGadget* msc, ReadInstrument* inst, TcpStreamer* tcp);

    /**
     * @brief Validate a sequence and start it at step 1 (enters Prepare).
     *
     * Every step must reference an existing media preset and experiment profile. Resets
     * step, repetition, part, run and split counters and the last error.
     *
     * @param seqId      Sequence id as in kSequences.
     * @param vehicle    Vehicle name stored with every capture.
     * @param profileId  Profile id stored with every capture.
     * @param autoArm    true to arm each step automatically after the settle time (3 s)
     *                   without waiting for the driver.
     * @param[out] err   Optional; set to "sequence_active", "offline_active",
     *                   "unknown_sequence" or "bad_step_definition" on failure.
     * @return true if the sequence was started.
     */
    bool start(const char* seqId, const String& vehicle, const String& profileId, bool autoArm,
               String* err);

    /** @brief Request arming of the waiting step (POST /api/sequence/next); consumed by poll(). */
    void requestNext() { nextRequested_ = true; }

    /**
     * @brief Skip the current step (BOOT long press or WebUI).
     *
     * Only effective in Prepare or WaitDriver. Moves to the next step, or to Done (and unpins
     * the serial number) after the last one.
     */
    void skip();

    /**
     * @brief Abort an active sequence and enter Failed.
     *
     * Disables the read stall, requests an OfflineCapture stop if capturing and unpins the
     * serial number. No-op if not active().
     *
     * @param reason  Error text stored as last error; nullptr stores "abort".
     */
    void abort(const char* reason);

    /**
     * @brief Advance the state machine; call periodically from the main loop.
     *
     * Prepare: wait for OfflineCapture to be idle, apply serial mode, switch media (not
     * persisted). WaitDriver: BOOT long = skip; after the settle time BOOT short, requestNext()
     * or auto_arm arms the step. Running: once the capture ends disable the stall; once
     * upload is done either split on overflow or advance().
     */
    void poll();

    /** @brief Current runner phase. */
    State state() const { return state_; }

    /** @brief True in Prepare, WaitDriver or Running. */
    bool active() const {
        return state_ == State::Prepare || state_ == State::WaitDriver || state_ == State::Running;
    }

    /** @brief True while the runner waits for the driver to arm the step (LED prompt). */
    bool waitingForDriver() const { return state_ == State::WaitDriver; }

    /**
     * @brief Serialize runner status for GET /api/sequence.
     *
     * Always writes state and active; with a sequence also id, label, step (1-based), steps,
     * rep (1-based), part, runs_done, splits, auto_arm, scenario, error (if any) and for the
     * current step media, experiment, prompt, serial_mode ("keep"/"policy") and stall.
     *
     * @param o  JSON object to fill.
     */
    void toJson(JsonObject o) const;

    /**
     * @brief Serialize all predefined sequences with their steps.
     *
     * Each entry: id, label, steps[] with media, experiment, measure_ms, repeats,
     * serial_mode, prompt and stall (raw JSON, if set).
     *
     * @param arr  JSON array to append to.
     */
    static void sequencesJson(JsonArray arr);

private:
    /** @brief Private: use instance(). */
    SequenceRunner() = default;

    /**
     * @brief Return the current step of the running sequence.
     * @return Pointer to the step, or nullptr without a sequence or past the last step.
     */
    const SequenceStep* step() const;

    /** @brief Enter Prepare for the current step and clear a pending next request. */
    void enterPrepare();

    /**
     * @brief Arm OfflineCapture for the current step and begin the capture.
     *
     * Measurement time is the step's measureMs (or the experiment's), halved for split
     * parts and clamped to OfflineCapture::maxCaptureMs(). Builds the scenario name
     * "<seq>_<step>r<rep>[p<part>]" and applies the step's stall JSON to ReadStall.
     *
     * @return true if the capture runs; false with lastError_ set to "arm_failed",
     *         "begin_failed" or "stall_config:<err>".
     */
    bool arm();

    /**
     * @brief Move to the next part, repetition or step after a finished run.
     *
     * Enters Done (and unpins the serial number) after the last step, otherwise Prepare.
     */
    void advance();

    /**
     * @brief Detect a BOOT button release (debounced, at least 30 ms held).
     * @param[out] longPress  Set to true if the button was held for at least 1500 ms; only
     *                        written when the function returns true.
     * @return true on a release edge.
     */
    bool bootEdge(bool* longPress);

    Session* session_ = nullptr;
    MscGadget* msc_ = nullptr;
    ReadInstrument* inst_ = nullptr;
    TcpStreamer* tcp_ = nullptr;

    const ExperimentSequence* seq_ = nullptr;
    State state_ = State::Idle;
    String vehicle_;
    String profileId_;
    String scenario_;
    String lastError_;
    bool autoArm_ = false;
    bool nextRequested_ = false;
    uint8_t stepIdx_ = 0;
    uint8_t rep_ = 0;
    uint8_t part_ = 0;        // 0 = whole step, 1/2 = split halves after overflow
    uint8_t runsDone_ = 0;
    uint8_t splits_ = 0;
    uint32_t stateSinceMs_ = 0;
    bool bootWasDown_ = false;
    uint32_t bootDownMs_ = 0;
};
