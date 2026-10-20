#include "SequenceRunner.h"
#include "BuildFlags.h"
#include "capture/OfflineCapture.h"
#include "media/ProbeMedia.h"
#include "msc/MscGadget.h"
#include "msc/ReadInstrument.h"
#include "msc/ReadStall.h"
#include "net/TcpStreamer.h"
#include "session/Session.h"

namespace {
constexpr uint32_t kSettleMs = 3000;      // HU sees the new LUN before the capture re-enum
constexpr uint32_t kBootLongMs = 1500;
constexpr uint32_t kBootMinMs = 30;

const char* stateName(SequenceRunner::State s) {
    switch (s) {
        case SequenceRunner::State::Prepare: return "prepare";
        case SequenceRunner::State::WaitDriver: return "wait_driver";
        case SequenceRunner::State::Running: return "running";
        case SequenceRunner::State::Done: return "done";
        case SequenceRunner::State::Failed: return "failed";
        default: return "idle";
    }
}
}  // namespace

SequenceRunner& SequenceRunner::instance() {
    static SequenceRunner r;
    return r;
}

void SequenceRunner::begin(Session* session, MscGadget* msc, ReadInstrument* inst,
                           TcpStreamer* tcp) {
    session_ = session;
    msc_ = msc;
    inst_ = inst;
    tcp_ = tcp;
}

const SequenceStep* SequenceRunner::step() const {
    if (!seq_ || stepIdx_ >= seq_->stepCount) return nullptr;
    return &seq_->steps[stepIdx_];
}

bool SequenceRunner::start(const char* seqId, const String& vehicle, const String& profileId,
                           bool autoArm, String* err) {
    if (active()) {
        if (err) *err = "sequence_active";
        return false;
    }
    if (OfflineCapture::instance().active()) {
        if (err) *err = "offline_active";
        return false;
    }
    const ExperimentSequence* s = findSequence(seqId);
    if (!s) {
        if (err) *err = "unknown_sequence";
        return false;
    }
    for (uint8_t i = 0; i < s->stepCount; i++) {
        if (!ProbeMedia::hasPreset(s->steps[i].media) ||
            !findExperiment(s->steps[i].experiment)) {
            if (err) *err = "bad_step_definition";
            return false;
        }
    }
    seq_ = s;
    vehicle_ = vehicle;
    profileId_ = profileId;
    autoArm_ = autoArm;
    stepIdx_ = 0;
    rep_ = 0;
    part_ = 0;
    runsDone_ = 0;
    splits_ = 0;
    lastError_ = "";
    Serial.printf("[SEQ] start %s steps=%u auto_arm=%d\n", s->id, s->stepCount, autoArm ? 1 : 0);
    enterPrepare();
    return true;
}

void SequenceRunner::enterPrepare() {
    nextRequested_ = false;
    state_ = State::Prepare;
    stateSinceMs_ = millis();
}

void SequenceRunner::skip() {
    if (state_ != State::Prepare && state_ != State::WaitDriver) return;
    Serial.printf("[SEQ] skip step %u\n", stepIdx_ + 1);
    part_ = 0;
    rep_ = 0;
    stepIdx_++;
    if (!step()) {
        state_ = State::Done;
        if (msc_) msc_->setSerialPinned(false);
        return;
    }
    enterPrepare();
}

void SequenceRunner::abort(const char* reason) {
    if (!active()) return;
    lastError_ = reason ? reason : "abort";
    ReadStall::instance().disable("seq_abort");
    if (OfflineCapture::instance().capturing()) OfflineCapture::instance().requestStop("seq_abort");
    if (msc_) msc_->setSerialPinned(false);
    state_ = State::Failed;
    Serial.printf("[SEQ] abort %s\n", lastError_.c_str());
}

bool SequenceRunner::bootEdge(bool* longPress) {
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
    const bool down = digitalRead(RESET_BUTTON_PIN) == LOW;
    bool fired = false;
    if (down && !bootWasDown_) bootDownMs_ = millis();
    if (!down && bootWasDown_) {
        const uint32_t held = millis() - bootDownMs_;
        if (held >= kBootMinMs) {
            fired = true;
            *longPress = held >= kBootLongMs;
        }
    }
    bootWasDown_ = down;
    return fired;
}

bool SequenceRunner::arm() {
    const SequenceStep* st = step();
    const ExperimentProfile* exp = st ? findExperiment(st->experiment) : nullptr;
    if (!st || !exp) return false;
    uint32_t measure = st->measureMs ? st->measureMs : exp->measureMs;
    if (part_ > 0) measure /= 2;
    if (measure > OfflineCapture::maxCaptureMs()) measure = OfflineCapture::maxCaptureMs();

    char sc[40];
    if (part_ > 0) {
        snprintf(sc, sizeof(sc), "%s_%ur%up%u", seq_->id, stepIdx_ + 1, rep_ + 1, part_);
    } else {
        snprintf(sc, sizeof(sc), "%s_%ur%u", seq_->id, stepIdx_ + 1, rep_ + 1);
    }
    scenario_ = sc;

    OfflineCapture& oc = OfflineCapture::instance();
    if (!oc.arm(session_, msc_, inst_, tcp_)) {
        lastError_ = "arm_failed";
        return false;
    }
    if (!oc.beginCapture(vehicle_, scenario_, profileId_, true, measure, exp->endStrategy,
                         exp->markers, exp->markerCount, true, false, st->fineData)) {
        oc.requestStop("begin_failed");
        lastError_ = "begin_failed";
        return false;
    }
    if (st->stall) {
        JsonDocument doc;
        String err;
        if (deserializeJson(doc, st->stall) ||
            !ReadStall::instance().configure(doc.as<JsonVariantConst>(), &err)) {
            oc.requestStop("stall_config");
            lastError_ = String("stall_config:") + err;
            return false;
        }
    }
    Serial.printf("[SEQ] run %s media=%s measure=%u stall=%d\n", scenario_.c_str(), st->media,
                  (unsigned)measure, st->stall ? 1 : 0);
    return true;
}

void SequenceRunner::advance() {
    const SequenceStep* st = step();
    if (!st) {
        state_ = State::Done;
        return;
    }
    runsDone_++;
    if (part_ == 1) {
        part_ = 2;
    } else {
        part_ = 0;
        if (++rep_ >= (st->repeats ? st->repeats : 1)) {
            rep_ = 0;
            stepIdx_++;
        }
    }
    if (!step()) {
        state_ = State::Done;
        if (msc_) msc_->setSerialPinned(false);
        Serial.printf("[SEQ] done %s runs=%u splits=%u\n", seq_->id, runsDone_, splits_);
        return;
    }
    enterPrepare();
}

void SequenceRunner::poll() {
    if (!active()) return;
    const uint32_t now = millis();
    switch (state_) {
        case State::Prepare: {
            const SequenceStep* st = step();
            if (!st) {
                state_ = State::Done;
                break;
            }
            if (OfflineCapture::instance().active()) break;
            if (msc_) msc_->setSerialPinned(st->serial == SeqSerial::Keep);
            if (strcmp(ProbeMedia::instance().variantId(), st->media) != 0) {
                String err;
                // Not persisted: a reboot mid-sequence falls back to the user's choice.
                if (!msc_ || !msc_->switchMedia(st->media, nullptr, false, &err, false)) {
                    abort(err.length() ? err.c_str() : "media_switch_failed");
                    break;
                }
            }
            state_ = State::WaitDriver;
            stateSinceMs_ = now;
            bootWasDown_ = false;
            Serial.printf("[SEQ] step %u/%u: %s\n", stepIdx_ + 1, seq_->stepCount, st->prompt);
            break;
        }
        case State::WaitDriver: {
            bool longPress = false;
            const bool boot = bootEdge(&longPress);
            if (boot && longPress) {
                skip();
                break;
            }
            const bool settled = now - stateSinceMs_ >= kSettleMs;
            if (settled && (boot || nextRequested_ || autoArm_)) {
                nextRequested_ = false;
                if (!arm()) {
                    abort(lastError_.c_str());
                    break;
                }
                state_ = State::Running;
                stateSinceMs_ = now;
            }
            break;
        }
        case State::Running: {
            OfflineCapture& oc = OfflineCapture::instance();
            if (oc.capturing()) break;
            // Brake must not outlive the capture window (finalize/upload need the USB idle).
            ReadStall::instance().disable("seq_step_end");
            if (oc.active()) break;
            if (oc.lastRunOverflow() > 0 && part_ == 0) {
                // Buffer full: same step again as two half-length runs.
                splits_++;
                part_ = 1;
                Serial.printf("[SEQ] overflow -> split step %u\n", stepIdx_ + 1);
                enterPrepare();
                break;
            }
            advance();
            break;
        }
        default:
            break;
    }
}

void SequenceRunner::toJson(JsonObject o) const {
    o["state"] = stateName(state_);
    o["active"] = active();
    if (!seq_) return;
    o["id"] = seq_->id;
    o["label"] = seq_->label;
    o["step"] = stepIdx_ + 1;
    o["steps"] = seq_->stepCount;
    o["rep"] = rep_ + 1;
    o["part"] = part_;
    o["runs_done"] = runsDone_;
    o["splits"] = splits_;
    o["auto_arm"] = autoArm_;
    o["scenario"] = scenario_;
    if (lastError_.length()) o["error"] = lastError_;
    const SequenceStep* st = step();
    if (st) {
        o["media"] = st->media;
        o["experiment"] = st->experiment;
        o["prompt"] = st->prompt;
        o["serial_mode"] = st->serial == SeqSerial::Keep ? "keep" : "policy";
        if (st->stall) o["stall"] = serialized(st->stall);
    }
}

void SequenceRunner::sequencesJson(JsonArray arr) {
    for (size_t i = 0; i < kSequenceCount; i++) {
        const ExperimentSequence& s = kSequences[i];
        JsonObject o = arr.add<JsonObject>();
        o["id"] = s.id;
        o["label"] = s.label;
        JsonArray steps = o["steps"].to<JsonArray>();
        for (uint8_t j = 0; j < s.stepCount; j++) {
            const SequenceStep& st = s.steps[j];
            JsonObject so = steps.add<JsonObject>();
            so["media"] = st.media;
            so["experiment"] = st.experiment;
            so["measure_ms"] = st.measureMs;
            so["repeats"] = st.repeats;
            so["serial_mode"] = st.serial == SeqSerial::Keep ? "keep" : "policy";
            so["prompt"] = st.prompt;
            if (st.stall) so["stall"] = serialized(st.stall);
        }
    }
}
