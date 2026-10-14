#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class Session {
public:
    void begin();
    bool start(const String& vehicle, const String& scenario, const String& profileId);
    void stop();
    bool active() const { return active_; }
    const String& id() const { return id_; }
    const String& vehicle() const { return vehicle_; }
    const String& scenario() const { return scenario_; }
    const String& profileId() const { return profileId_; }
    const String& phase() const { return phase_; }
    void setPhase(const String& phase, const String& note = "");
    uint32_t nextSeq() { return ++seq_; }
    uint32_t seq() const { return seq_; }
    void marker(const String& name, const String& note = "");
    void toStatusJson(JsonObject obj) const;

private:
    bool active_ = false;
    String id_;
    String vehicle_;
    String scenario_;
    String profileId_;
    String phase_ = "idle";
    uint32_t seq_ = 0;
    unsigned long startedMs_ = 0;
};
