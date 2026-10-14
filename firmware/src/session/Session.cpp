#include "Session.h"
#include "BuildFlags.h"
#include "core/ProbeClock.h"
#include "net/EventBus.h"
#include <WiFi.h>
#include <time.h>

void Session::begin() {
    active_ = false;
    id_ = "";
    phase_ = "idle";
    seq_ = 0;
    seqFirst_ = 0;
}

bool Session::start(const String& vehicle, const String& scenario, const String& profileId) {
    if (active_) stop();
    vehicle_ = vehicle;
    scenario_ = scenario;
    profileId_ = profileId;
    seq_ = 0;
    startedMs_ = millis();
    phase_ = "enum";

    char buf[64];
    time_t now = time(nullptr);
    struct tm tm;
    if (now > 100000 && localtime_r(&now, &tm)) {
        snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d%02d%02d_%s_%s",
                 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                 tm.tm_hour, tm.tm_min, tm.tm_sec,
                 vehicle.c_str(), scenario.c_str());
    } else {
        snprintf(buf, sizeof(buf), "%lu_%s_%s",
                 (unsigned long)startedMs_, vehicle.c_str(), scenario.c_str());
    }
    id_ = buf;
    active_ = true;

    JsonDocument doc;
    int64_t t = ProbeClock::nowUs();
    doc["v"] = 2;
    doc["sid"] = id_;
    doc["seq"] = nextSeq();
    seqFirst_ = seq_;
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "session";
    doc["action"] = "start";
    doc["vehicle"] = vehicle_;
    doc["scenario"] = scenario_;
    doc["profile_id"] = profileId_;
    doc["fw_version"] = FW_VERSION;
    doc["fw_type"] = FW_TYPE;
    // Explicit host class so lab Debian can never be mistaken for a car HU.
    if (vehicle_ == "lab_debian") {
        doc["usb_host_kind"] = "linux_debian";
        doc["usb_host_role"] = "lab_reference";
    } else if (vehicle_ == "bmw") {
        doc["usb_host_kind"] = "bmw_nbt_evo";
        doc["usb_host_role"] = "vehicle_hu";
    } else if (vehicle_ == "mazda_cx30") {
        doc["usb_host_kind"] = "mazda_cx30";
        doc["usb_host_role"] = "vehicle_hu";
    } else {
        doc["usb_host_kind"] = "unknown";
        doc["usb_host_role"] = "unknown";
    }
    {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        doc["mac"] = macStr;
    }
    EventBus::instance().publish(doc);
    return true;
}

void Session::stop() {
    if (!active_) return;
    JsonDocument doc;
    int64_t t = ProbeClock::nowUs();
    doc["v"] = 2;
    doc["sid"] = id_;
    doc["seq"] = nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "session";
    doc["action"] = "stop";
    EventBus::instance().publish(doc);
    active_ = false;
    phase_ = "idle";
}

void Session::setPhase(const String& phase, const String& note) {
    phase_ = phase;
    if (!active_) return;
    JsonDocument doc;
    int64_t t = ProbeClock::nowUs();
    doc["v"] = 2;
    doc["sid"] = id_;
    doc["seq"] = nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "phase";
    doc["phase"] = phase_;
    if (note.length()) doc["note"] = note;
    EventBus::instance().publish(doc);
}

void Session::marker(const String& name, const String& note) {
    if (!active_) return;
    JsonDocument doc;
    int64_t t = ProbeClock::nowUs();
    doc["v"] = 2;
    doc["sid"] = id_;
    doc["seq"] = nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "marker";
    doc["name"] = name;
    if (note.length()) doc["note"] = note;
    EventBus::instance().publish(doc);
}

void Session::toStatusJson(JsonObject obj) const {
    obj["active"] = active_;
    obj["session_id"] = id_;
    obj["vehicle"] = vehicle_;
    obj["scenario"] = scenario_;
    obj["profile_id"] = profileId_;
    obj["phase"] = phase_;
    obj["seq"] = seq();
    obj["seq_first"] = seqFirst_;
}

uint32_t Session::nextSeq() {
    portENTER_CRITICAL(&seqMux_);
    uint32_t s = ++seq_;
    portEXIT_CRITICAL(&seqMux_);
    return s;
}

uint32_t Session::seq() const {
    portENTER_CRITICAL(&seqMux_);
    uint32_t s = seq_;
    portEXIT_CRITICAL(&seqMux_);
    return s;
}

uint32_t Session::seqFirst() const {
    return seqFirst_;
}
