#pragma once

/**
 * @file SessionMetricsJson.h
 * @brief ArduinoJson serialization for SessionMetrics (UNKNOWN → null, never fake 0).
 */

#include "session/SessionMetrics.h"

#include <ArduinoJson.h>

namespace huprobe {

inline void metricToJson(JsonObject obj, const char* key, const MetricU32& m) {
    if (m.isUnknown()) {
        obj[key] = nullptr;
    } else {
        obj[key] = m.value;
    }
}

/**
 * @brief Fill session_metrics + timeline into a JSON object.
 * @param obj  Target (e.g. doc["session_metrics"]).
 * @param sm   Metrics instance.
 * @param max_tl  Max timeline entries to emit (tail if oversized).
 */
inline void sessionMetricsToJson(JsonObject obj, const SessionMetrics& sm, size_t max_tl = 32) {
    const char* plane = "none";
    if (sm.plane() == SessionMetrics::Plane::Msc) plane = "msc";
    else if (sm.plane() == SessionMetrics::Plane::AoaVendor) plane = "aoa_vendor";
    obj["plane"] = plane;
    obj["visibility_scope"] = sm.visibilityScope();
    obj["is_full_ep0_trace"] = sm.isFullEp0Trace();
    obj["identity_vid"] = sm.identityVid();
    obj["identity_pid"] = sm.identityPid();
    if (sm.identityNote()[0]) obj["identity_note"] = sm.identityNote();

    metricToJson(obj, "attach_count", sm.attach());
    metricToJson(obj, "detach_count", sm.detach());
    metricToJson(obj, "reconnect_count", sm.reconnect());
    metricToJson(obj, "enumeration_count", sm.enumeration());
    metricToJson(obj, "configuration_count", sm.configuration());
    metricToJson(obj, "control_setup_count", sm.controlSetup());
    metricToJson(obj, "control_in_count", sm.controlIn());
    metricToJson(obj, "control_out_count", sm.controlOut());
    metricToJson(obj, "vendor_setup_count", sm.vendorSetup());
    metricToJson(obj, "aoa_51", sm.aoa51());
    metricToJson(obj, "aoa_52", sm.aoa52());
    metricToJson(obj, "aoa_53", sm.aoa53());
    metricToJson(obj, "aoa51_respond_ok", sm.aoa51Ok());
    metricToJson(obj, "bulk_rx_count", sm.bulkRx());
    metricToJson(obj, "bulk_tx_count", sm.bulkTx());
    metricToJson(obj, "rx_bytes", sm.rxBytes());
    metricToJson(obj, "tx_bytes", sm.txBytes());
    metricToJson(obj, "scsi_command_count", sm.scsiCommand());
    metricToJson(obj, "read_count", sm.readCount());
    metricToJson(obj, "read_bytes", sm.readBytes());
    metricToJson(obj, "write_count", sm.writeCount());
    metricToJson(obj, "write_bytes", sm.writeBytes());
    metricToJson(obj, "reset_count", sm.resetCount());
    metricToJson(obj, "clear_halt_count", sm.clearHalt());

    // READ size / LBA histograms: not aggregated on-device → explicit UNKNOWN.
    obj["read_size_histogram"] = nullptr;
    obj["read_lba_histogram"] = nullptr;

    if (sm.scsiHistArmed()) {
        JsonObject hist = obj["scsi_opcode_histogram"].to<JsonObject>();
        for (size_t i = 0; i < SessionMetrics::kHistCount; ++i) {
            hist[SessionMetrics::scsiHistName(static_cast<SessionMetrics::ScsiHist>(i))] =
                sm.scsiHist(static_cast<SessionMetrics::ScsiHist>(i));
        }
    } else {
        obj["scsi_opcode_histogram"] = nullptr;
    }

    const UsbSessionTimeline& tl = sm.timeline();
    if (tl.hasTiming()) {
        obj["first_event_ms"] = tl.firstEventMs();
        obj["last_event_ms"] = tl.lastEventMs();
        obj["session_duration_ms"] = tl.durationMs();
        if (tl.sessionId()[0]) obj["timeline_session_id"] = tl.sessionId();
    } else {
        obj["first_event_ms"] = nullptr;
        obj["last_event_ms"] = nullptr;
        obj["session_duration_ms"] = nullptr;
    }
    obj["timeline_overflow"] = tl.overflowCount();
    obj["timeline_size"] = (unsigned)tl.size();

    JsonArray arr = obj["timeline"].to<JsonArray>();
    const size_t n = tl.size();
    size_t start = 0;
    if (n > max_tl) start = n - max_tl;
    for (size_t i = start; i < n; ++i) {
        UsbTlEntry e;
        if (!tl.at(i, e)) break;
        JsonObject row = arr.add<JsonObject>();
        row["t_ms"] = e.t_ms;
        row["rel_ms"] = tl.relativeMs(e.t_ms);
        row["ev"] = usbTlEventName(e.type);
        if (e.detail) row["detail"] = e.detail;
    }

    obj["unknown_means"] =
        "null_fields_are_not_observable_on_this_instrument_layer_zero_means_measured_absent";
}

}  // namespace huprobe
