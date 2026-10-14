#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class Session {
public:
    void begin();
    bool start(const String& vehicle, const String& scenario, const String& profileId,
               const char* probeSerial = nullptr, const char* volumeLabel = nullptr);
    void stop();
    bool active() const { return active_; }
    const String& id() const { return id_; }
    const String& vehicle() const { return vehicle_; }
    const String& scenario() const { return scenario_; }
    const String& profileId() const { return profileId_; }
    const String& phase() const { return phase_; }
    void setPhase(const String& phase, const String& note = "");
    uint32_t nextSeq();
    uint32_t seq() const;
    uint32_t seqFirst() const;
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
    uint32_t seqFirst_ = 0;
    unsigned long startedMs_ = 0;
    mutable portMUX_TYPE seqMux_ = portMUX_INITIALIZER_UNLOCKED;
};
