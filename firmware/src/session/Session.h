#pragma once

/**
 * @file Session.h
 * @brief Measurement session: identity, phase, event sequence numbers and session/phase/marker events.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief One measurement session; emits v2 session/phase/marker events to the EventBus.
 *
 * The event sequence counter is guarded by a portMUX critical section so events from
 * different tasks get unique, increasing seq numbers.
 */
class Session {
public:
    /** @brief Reset to the idle state (inactive, empty id, phase "idle", seq 0). */
    void begin();

    /**
     * @brief Start a new session and publish the "session start" event.
     *
     * Stops a running session first. The id is "YYYYMMDD-HHMMSS_<vehicle>_<scenario>" when the
     * wall clock is set, otherwise "<millis>_<vehicle>_<scenario>". The start event carries
     * firmware/build info, probe identity, media info and the USB host class derived from the
     * vehicle (lab_debian, bmw, mazda_cx30, else unknown); the media layout is published right
     * after (B21). Raises the ProbeClock watermark so deferred Capture-tUs cannot stamp below start.
     *
     * @param vehicle      Vehicle id (also part of the session id).
     * @param scenario     Scenario id (also part of the session id).
     * @param profileId    Profile id.
     * @param probeSerial  USB serial reported to the host, or nullptr for "".
     * @param volumeLabel  MSC volume label, or nullptr for "".
     * @param transport    Transport tag, e.g. "tcp_live"; nullptr falls back to "tcp_live".
     * @param wallclockUtc Optional UTC wall-clock string added as wallclock_utc; nullptr/empty to omit.
     * @return Always true.
     */
    bool start(const String& vehicle, const String& scenario, const String& profileId,
               const char* probeSerial = nullptr, const char* volumeLabel = nullptr,
               const char* transport = "tcp_live", const char* wallclockUtc = nullptr);

    /** @brief Publish the "session stop" event and return to idle; no-op if no session is active. */
    void stop();

    /** @brief True between start() and stop(). */
    bool active() const { return active_; }

    /** @brief Session id of the current (or last) session; empty after begin(). */
    const String& id() const { return id_; }

    /** @brief Vehicle id passed to start(). */
    const String& vehicle() const { return vehicle_; }

    /** @brief Scenario id passed to start(). */
    const String& scenario() const { return scenario_; }

    /** @brief Profile id passed to start(). */
    const String& profileId() const { return profileId_; }

    /** @brief Current phase name ("idle", "enum" after start, then as set via setPhase()). */
    const String& phase() const { return phase_; }

    /** @brief Transport tag of the session (e.g. "tcp_live"). */
    const String& transport() const { return transport_; }

    /**
     * @brief Set the phase and, if a session is active, publish a "phase" event.
     *
     * Optional tUs aligns phase with Capture epochs (e.g. mount). 0 = now.
     *
     * @param phase New phase name; stored even without an active session.
     * @param note  Optional note added to the event if non-empty.
     * @param tUs   Capture timestamp in microseconds; 0 (or negative) = now.
     */
    void setPhase(const String& phase, const String& note = "", int64_t tUs = 0);

    /**
     * @brief Increment and return the event sequence number (thread-safe).
     * @return The new sequence number (first event of a session gets 1).
     */
    uint32_t nextSeq();

    /**
     * @brief Return the last assigned event sequence number (thread-safe read).
     * @return Current sequence number; 0 before the first event.
     */
    uint32_t seq() const;

    /**
     * @brief Return the sequence number of the session start event.
     * @return First seq of the current session.
     */
    uint32_t seqFirst() const;

    /**
     * @brief Publish a "marker" event; no-op without an active session.
     *
     * Optional tUs = Capture time (offline replay); 0 = now.
     *
     * @param name Marker name.
     * @param note Optional note added to the event if non-empty.
     * @param tUs  Capture timestamp in microseconds; 0 (or negative) = now.
     */
    void marker(const String& name, const String& note = "", int64_t tUs = 0);

    /**
     * @brief Write the session status (active, id, vehicle, scenario, profile, phase, transport, seq) into a JSON object.
     * @param[out] obj Target JSON object.
     */
    void toStatusJson(JsonObject obj) const;

private:
    bool active_ = false;
    String id_;
    String vehicle_;
    String scenario_;
    String profileId_;
    String phase_ = "idle";
    String transport_ = "tcp_live";
    uint32_t seq_ = 0;
    uint32_t seqFirst_ = 0;
    unsigned long startedMs_ = 0;
    mutable portMUX_TYPE seqMux_ = portMUX_INITIALIZER_UNLOCKED;
};
