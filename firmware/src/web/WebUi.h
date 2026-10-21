#pragma once

/**
 * @file WebUi.h
 * @brief HTTP server on port 80: single-page WebUI, JSON REST API and OTA upload.
 */

#include <WebServer.h>
#include "core/ConfigStore.h"
#include "session/Session.h"
#include "msc/ReadInstrument.h"
#include "msc/MscGadget.h"
#include "core/HubClient.h"
#include "net/TcpStreamer.h"

/**
 * @brief Web interface of the probe: serves the WebUI page and all /api/* endpoints.
 *
 * Handlers run synchronously inside loop() on the Arduino loop task. JSON error responses
 * use the shape {"ok":false,"err":"<code>"}.
 */
class WebUi {
public:
    /**
     * @brief Store the collaborators, register all routes and start the server on port 80.
     * @param cfg      Persistent configuration (read and saved by several handlers).
     * @param session  Live capture session.
     * @param inst     READ instrumentation (rates, simulated reads).
     * @param msc      MSC gadget (USB/media control).
     * @param hub      Hub client, used for the hub_ok status flag.
     * @param tcp      TCP streamer, used for tcp_ok and passed to OfflineCapture.
     */
    void begin(ConfigStore* cfg, Session* session, ReadInstrument* inst,
               MscGadget* msc, HubClient* hub, TcpStreamer* tcp);

    /**
     * @brief Serve pending HTTP clients and drive a running "upload all trips" job.
     * @note Call from the Arduino loop task.
     */
    void loop();

private:
    /** @brief GET /: send the embedded single-page WebUI (text/html). */
    void handleRoot();

    /**
     * @brief GET /api/status: full device status as JSON (polled by the WebUI every second).
     *
     * Fields: fw, fwType, board, hw{chip, chip_rev, cpu_mhz, flash/psram/heap/sketch sizes,
     * HwInfo}, previous-boot panic info, this-boot panic_now, diag ring/snapshot counters,
     * ip, hub_ok, tcp_ok, msc_* counters and flags,
     * plug_count, unplug_count, read_rate_kb_s, tur_hz, read_count, session, vehicle,
     * scenario, collector ("host:port"), offline, media, sequence, stall, drive, ota, tur.
     * Always HTTP 200.
     */
    void handleStatus();

    /** @brief GET /api/config: current ConfigStore as JSON (empty object without config). */
    void handleConfigGet();

    /**
     * @brief POST /api/config: merge the JSON body into ConfigStore and save it.
     *
     * Responds {"ok":true}; HTTP 400 text/plain "bad request" without config or body,
     * "json" on a parse error.
     */
    void handleConfigPost();

    /**
     * @brief POST /api/session/start: start a live session.
     *
     * Optional body {vehicle, scenario, profile_id}; missing fields default to the config.
     * Without profile_id the profile is derived from the vehicle. The values are saved to
     * the config; the session gets the current USB serial and volume label. Responds
     * {"ok":true} (HTTP 200).
     */
    void handleSessionStart();

    /** @brief POST /api/session/stop: stop the live session; always responds {"ok":true}. */
    void handleSessionStop();

    /**
     * @brief POST /api/marker: record an operator marker.
     *
     * Optional body {name (default "operator"), note}. While an offline capture is
     * recording, the name is queued into the RAM capture (response "via":"offline_ram",
     * note dropped); otherwise it goes to the live session. Always HTTP 200.
     */
    void handleMarker();

    /**
     * @brief POST /api/debug/sim-read: inject one synthetic attach + READ10 CBW/DATA/CSW.
     *
     * Debug aid for the pipeline without a host. Responds {"ok":true,"schema":2}, or
     * {"ok":false} (still HTTP 200) without ReadInstrument.
     */
    void handleSimulateRead();

    /**
     * @brief POST /api/usb/cycle: soft USB re-enumeration via the D+ pull-up (not VBUS).
     *
     * Optional body {holdMs} (default 600 ms). Responds {ok, action, holdMs, note}; HTTP 500
     * if the cycle failed or no MSC gadget is present.
     */
    void handleUsbCycle();

    /**
     * @brief POST /api/usb/remount: remount the medium without a USB re-enumeration.
     *
     * Optional body {bumpSerial} (default true). Responds {ok:true, action, serial}
     * (HTTP 200).
     */
    void handleUsbRemount();

    /**
     * @brief POST /api/offline/start: arm a manual offline capture (WiFi goes off).
     *
     * Optional body {vehicle, scenario, profile_id, soft_usb, auto_detach_ms or measure_ms,
     * end_strategy ("media_hide"|"soft_detach"), fine_data}; defaults from the config. If
     * the scenario matches an experiment profile, its markers, end-on-unmount and free-marker
     * settings are used and its soft_usb, end_strategy and measure time fill fields not given.
     * HTTP 409 "already_active" if a capture runs; see armOfflineCapture() for the rest.
     */
    void handleOfflineStart();

    /**
     * @brief POST /api/offline/stop: request a stop of the recording offline capture.
     *
     * WiFi is off during capture, so this only works if stop is triggered while somehow
     * still connected (e.g. race) or after reconnect; prefer Unplug/BOOT. HTTP 409
     * "not_capturing" if nothing records, else {ok:true, action:"offline_stop_requested"}.
     */
    void handleOfflineStop();

    /**
     * @brief GET /api/experiments: list experiment profiles and the capture budget.
     *
     * Each entry: id, label, scenario, measure_ms, soft_usb, end_strategy, end_on_unmount,
     * free_markers, markers[]. Plus capture_budget from OfflineCapture. HTTP 200.
     */
    void handleExperiments();

    /**
     * @brief POST /api/experiment/start: arm an offline capture from an experiment profile.
     *
     * Body {experiment or scenario (default "free"), vehicle, profile_id, soft_usb,
     * measure_ms or auto_detach_ms, end_strategy, end_on_unmount, fine_data}; fields not
     * given come from the profile; measure time is capped to OfflineCapture::maxCaptureMs().
     * HTTP 409 "already_active", HTTP 400 "unknown_experiment"; see armOfflineCapture().
     */
    void handleExperimentStart();

    /** @brief GET /api/media: active medium (with details) plus all presets as JSON. */
    void handleMediaGet();

    /** @brief GET /api/media/presets: list of media presets as {"presets":[...]}. */
    void handleMediaPresets();

    /**
     * @brief POST /api/media/variant: switch the presented medium to a preset or ad-hoc spec.
     *
     * Body {preset | spec (object), method: "cycle" (default, host re-enumerates) |
     * "remount" (NOT READY window only)}. Responds {ok, method, err?, active, usb_serial}.
     * HTTP 409 "capture_or_sequence_active", HTTP 400 "json", "preset_or_spec_required",
     * switch error or "msc_not_ready".
     */
    void handleMediaVariant();

    /**
     * @brief POST /api/media/mutate: change medium content in place, optionally delayed.
     *
     * Body: mutation fields for ProbeMedia::parseMutation, notify ("none", "unit_attention",
     * "remount", "cycle"; default "none") and delay_s. Offline capture turns WiFi off, so
     * arm with delay_s to land the change mid-capture. Responds {ok, err?, notify,
     * scheduled, due_in_ms, active}; HTTP 400 on JSON, notify or mutation errors.
     */
    void handleMediaMutate();

    /**
     * @brief POST /api/media/notify: signal a media change to the host without changing data.
     *
     * Optional body {kind: "unit_attention" (default) | "remount" | "cycle"}. Responds
     * {ok, kind, ua_pending, ua_sent}; HTTP 400 for an invalid or "none" kind, HTTP 500 if
     * the notification failed.
     */
    void handleMediaNotify();

    /** @brief GET /api/stall: current ReadStall configuration as {"stall":{...}}. */
    void handleStallGet();

    /**
     * @brief POST /api/stall: configure or disable the READ stall (brake).
     *
     * Body is the ReadStall configuration; {"enable":false} disables it. Responds
     * {ok, err?, stall}; HTTP 400 on JSON or configuration errors and "sequence_active"
     * while a sequence controls the stall.
     */
    void handleStallPost();

    /** @brief GET /api/sequences: all predefined sequences plus the runner status. */
    void handleSequences();

    /**
     * @brief POST /api/sequence/start: start an experiment sequence.
     *
     * Body {id, vehicle, profile_id, auto_arm}; vehicle/profile default to the config and
     * the profile is derived from the vehicle unless given. Responds {ok, err?, runner};
     * HTTP 409 on start errors (see SequenceRunner::start()).
     */
    void handleSequenceStart();

    /**
     * @brief POST /api/sequence/next|skip|abort: send a command to the sequence runner.
     *
     * Always responds HTTP 200 {ok:true, cmd, runner}.
     *
     * @param cmd  "next" (arm the waiting step), "skip"; any other value aborts with
     *             reason "api_abort".
     */
    void handleSequenceCmd(const char* cmd);

    /**
     * @brief GET /api/diag/log: diagnostic log as chunked plain text.
     *
     * Plain text: header line (reset reason, restart cause, boot generation, uptime), RTC
     * tail of the previous boot, then the PSRAM ring of this boot. Query `tail=<bytes>`
     * skips the previous-boot section and sends only the last N bytes of this boot.
     */
    void handleDiagLog();

    /**
     * @brief GET /api/diag/saved: flash snapshots newest first as chunked plain text.
     *
     * Each snapshot starts with `# snapshot seq=…`. Empty body with a header if none.
     */
    void handleDiagSaved();

    /**
     * @brief POST /api/diag/persist: force a flash snapshot of the current log.
     *
     * Responds {ok, err?}. HTTP 409 when persist is unavailable (no partition / buffer).
     */
    void handleDiagPersist();

    /**
     * @brief GET /api/trips: list stored drive-mode trips.
     *
     * Body is TripStore::toJson() plus upload_all (upload job running) and upload_error
     * (last job error, if any). HTTP 200.
     */
    void handleTrips();

    /**
     * @brief POST /api/trips/upload: start uploading all pending trips to the collector.
     *
     * Responds {ok:true, pending}; the upload runs in pumpTripUpload(). HTTP 409 with err
     * "no_trips_partition", "busy" (capture or upload active), "no_wifi" or
     * "collector_disabled".
     */
    void handleTripsUpload();

    /**
     * @brief POST /api/trips/erase: erase the trip journal.
     *
     * Body {all}: true erases everything; false (default) erases only if every trip is
     * uploaded, because the journal is append-only. Responds {ok, err?, trips}; HTTP 409
     * "no_trips_partition", "busy" (writing, capture or upload active),
     * "pending_trips_left" or a failed erase.
     */
    void handleTripsErase();

    /**
     * @brief Upload one stored trip after the other via OfflineCapture::replayStored.
     *
     * Called from loop() while the job runs; waits while OfflineCapture is active. Ends when
     * nothing is pending, the same trip is still pending after its replay
     * ("trip_upload_incomplete") or a replay could not start ("replay_failed"); then
     * restarts the live session if it was running when the job started.
     */
    void pumpTripUpload();

    /**
     * @brief Serialize a JSON document and send it as application/json.
     * @param code  HTTP status code.
     * @param doc   Document to send.
     */
    void sendJson(int code, JsonDocument& doc);

    /**
     * @brief POST /ota-upload (upload callback): stream the firmware image into Update.
     *
     * Called per multipart chunk (start, write, end); errors are printed to Serial and
     * reported by handleOtaDone().
     */
    void handleOtaUpload();

    /**
     * @brief POST /ota-upload (completion): report the OTA result and reboot on success.
     *
     * Responds text/plain "OK" then restarts after 400 ms (restart cause RestartWebUi), or
     * HTTP 500 "OTA fail".
     */
    void handleOtaDone();

    /**
     * @brief Shared arm path for /api/offline/start and /api/experiment/start.
     *
     * Rejects with HTTP 409 "already_active" or "sequence_active". Otherwise saves vehicle,
     * scenario, profile and end strategy to the config, sends the HTTP 200 response
     * {ok, action:"offline_arm", experiment?, vehicle, scenario, profile_id, soft_usb,
     * auto_detach_ms, measure_ms, end_strategy, end_on_unmount, free_markers, fine_data,
     * note, markers[], capture_budget (theoretical, before allocation)} and flushes it
     * before arming, because arming turns WiFi off. Arm/begin failures are only logged.
     *
     * @param vehicle       Vehicle id.
     * @param scenario      Session scenario id.
     * @param profile       Profile id.
     * @param softUsb       true to soft re-enumerate USB inside the WiFi-off window.
     * @param measureMs     Timed end of the capture in milliseconds.
     * @param endStrategy   "media_hide" or "soft_detach".
     * @param markers       Operator markers cycled by BOOT short; may be nullptr.
     * @param markerCount   Number of entries in @p markers.
     * @param experimentId  Experiment profile id echoed in the response; may be nullptr.
     * @param endOnUnmount  true if unmount/unplug ends the capture.
     * @param freeMarkers   true for BOOT short = mark_NN, BOOT double = soft replug.
     * @param fineData      true for one DATA record per 4 KiB callback (chunk timing).
     */
    void armOfflineCapture(const String& vehicle, const String& scenario,
                           const String& profile, bool softUsb, uint32_t measureMs,
                           const String& endStrategy, const char* const* markers,
                           uint8_t markerCount, const char* experimentId,
                           bool endOnUnmount = true, bool freeMarkers = false,
                           bool fineData = false);

    WebServer server_{80};
    ConfigStore* cfg_ = nullptr;
    Session* session_ = nullptr;
    ReadInstrument* inst_ = nullptr;
    MscGadget* msc_ = nullptr;
    HubClient* hub_ = nullptr;
    TcpStreamer* tcp_ = nullptr;
    bool tripUploadAll_ = false;
    bool tripUploadRestoreLive_ = false;
    uint32_t tripUploadLastId_ = 0;
    String tripUploadErr_;
};
