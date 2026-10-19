#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "capture/ExperimentProfiles.h"

class Session;
class MscGadget;
class ReadInstrument;
class TcpStreamer;

/**
 * Runs an ExperimentSequence step by step (0.2.31, R1).
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
    enum class State : uint8_t { Idle = 0, Prepare, WaitDriver, Running, Done, Failed };

    static SequenceRunner& instance();

    void begin(Session* session, MscGadget* msc, ReadInstrument* inst, TcpStreamer* tcp);
    bool start(const char* seqId, const String& vehicle, const String& profileId, bool autoArm,
               String* err);
    void requestNext() { nextRequested_ = true; }
    void skip();
    void abort(const char* reason);
    void poll();

    State state() const { return state_; }
    bool active() const {
        return state_ == State::Prepare || state_ == State::WaitDriver || state_ == State::Running;
    }
    bool waitingForDriver() const { return state_ == State::WaitDriver; }
    void toJson(JsonObject o) const;
    static void sequencesJson(JsonArray arr);

private:
    SequenceRunner() = default;
    const SequenceStep* step() const;
    void enterPrepare();
    bool arm();
    void advance();
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
