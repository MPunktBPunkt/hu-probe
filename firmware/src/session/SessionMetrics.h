#pragma once

/**
 * @file SessionMetrics.h
 * @brief Quantitative USB session counters with explicit UNKNOWN vs measured-0.
 *
 * Method origin: esp32.usb-analyzer field host-matrix fingerprinting (2026-11-06).
 * Only quantities observable on the hu-probe L1/L2 instrument layer are MEASURED.
 * Bus-level EP0 outside USBVendor.onRequest stays UNKNOWN — never reported as 0.
 */

#include "session/UsbSessionTimeline.h"

#include <cstdint>
#include <cstring>

namespace huprobe {

/** @brief Measurement plane for a metric field. */
enum class MetricVis : uint8_t {
    Measured = 0,  ///< Counter observed on this instrument layer (0 = none seen).
    Unknown = 1,   ///< Not observable on this layer — must not appear as 0.
};

struct MetricU32 {
    MetricVis vis = MetricVis::Unknown;
    uint32_t value = 0;

    static MetricU32 measured(uint32_t v) {
        MetricU32 m;
        m.vis = MetricVis::Measured;
        m.value = v;
        return m;
    }
    static MetricU32 unknown() {
        MetricU32 m;
        m.vis = MetricVis::Unknown;
        m.value = 0;
        return m;
    }
    bool isUnknown() const { return vis == MetricVis::Unknown; }
};

/**
 * @brief Aggregated session metrics for AOA and/or MSC gadget boots.
 *
 * Call setPlane() at boot. Counters start UNKNOWN and become Measured when the
 * corresponding instrument is armed / first note*() runs.
 */
class SessionMetrics {
public:
    SessionMetrics() { reset(); }

    static SessionMetrics& instance() {
        static SessionMetrics m;
        return m;
    }

    enum class Plane : uint8_t { None = 0, Msc = 1, AoaVendor = 2 };

    void reset() {
        plane_ = Plane::None;
        identity_vid_ = identity_pid_ = 0;
        identity_note_[0] = '\0';
        visibility_scope_[0] = '\0';
        is_full_ep0_trace_ = false;

        attach_ = MetricU32::unknown();
        detach_ = MetricU32::unknown();
        reconnect_ = MetricU32::unknown();
        enumeration_ = MetricU32::unknown();
        configuration_ = MetricU32::unknown();
        control_setup_ = MetricU32::unknown();
        control_in_ = MetricU32::unknown();
        control_out_ = MetricU32::unknown();
        vendor_setup_ = MetricU32::unknown();
        aoa51_ = aoa52_ = aoa53_ = aoa51_ok_ = MetricU32::unknown();
        bulk_rx_ = bulk_tx_ = MetricU32::unknown();
        rx_bytes_ = tx_bytes_ = MetricU32::unknown();
        scsi_command_ = MetricU32::unknown();
        for (auto& c : scsi_hist_) c = 0;
        scsi_hist_armed_ = false;
        read_count_ = read_bytes_ = MetricU32::unknown();
        write_count_ = write_bytes_ = MetricU32::unknown();
        reset_count_ = clear_halt_ = MetricU32::unknown();
        timeline_.reset();
    }

    void setPlaneAoa(const char* scope = "usbvendor_onrequest_only") {
        plane_ = Plane::AoaVendor;
        std::strncpy(visibility_scope_, scope ? scope : "usbvendor_onrequest_only",
                     sizeof(visibility_scope_) - 1);
        visibility_scope_[sizeof(visibility_scope_) - 1] = '\0';
        is_full_ep0_trace_ = false;
        // Arm measurable AOA counters at 0; leave bus-only fields UNKNOWN.
        attach_ = MetricU32::measured(0);
        detach_ = MetricU32::measured(0);
        reconnect_ = MetricU32::measured(0);
        control_setup_ = MetricU32::measured(0);
        vendor_setup_ = MetricU32::measured(0);
        aoa51_ = aoa52_ = aoa53_ = aoa51_ok_ = MetricU32::measured(0);
        bulk_rx_ = bulk_tx_ = MetricU32::measured(0);
        rx_bytes_ = tx_bytes_ = MetricU32::measured(0);
        // Not observable on vendor-callback plane:
        enumeration_ = MetricU32::unknown();
        configuration_ = MetricU32::unknown();
        control_in_ = MetricU32::unknown();
        control_out_ = MetricU32::unknown();
        scsi_command_ = MetricU32::unknown();
        scsi_hist_armed_ = false;
        read_count_ = read_bytes_ = MetricU32::unknown();
        write_count_ = write_bytes_ = MetricU32::unknown();
        reset_count_ = clear_halt_ = MetricU32::unknown();
    }

    void setPlaneMsc(const char* scope = "mscd_bot_cbw") {
        plane_ = Plane::Msc;
        std::strncpy(visibility_scope_, scope ? scope : "mscd_bot_cbw",
                     sizeof(visibility_scope_) - 1);
        visibility_scope_[sizeof(visibility_scope_) - 1] = '\0';
        is_full_ep0_trace_ = false;
        attach_ = MetricU32::measured(0);  // plug_count mapped by caller
        detach_ = MetricU32::measured(0);
        reconnect_ = MetricU32::measured(0);
        scsi_command_ = MetricU32::measured(0);
        scsi_hist_armed_ = true;
        for (auto& c : scsi_hist_) c = 0;
        read_count_ = read_bytes_ = MetricU32::measured(0);
        write_count_ = write_bytes_ = MetricU32::measured(0);
        // AOA / vendor control not on MSC golden path:
        control_setup_ = MetricU32::unknown();
        vendor_setup_ = MetricU32::unknown();
        aoa51_ = aoa52_ = aoa53_ = aoa51_ok_ = MetricU32::unknown();
        bulk_rx_ = bulk_tx_ = MetricU32::unknown();
        rx_bytes_ = tx_bytes_ = MetricU32::unknown();
        enumeration_ = MetricU32::unknown();
        configuration_ = MetricU32::unknown();
        control_in_ = MetricU32::unknown();
        control_out_ = MetricU32::unknown();
        reset_count_ = MetricU32::unknown();
        clear_halt_ = MetricU32::unknown();
    }

    void setIdentity(uint16_t vid, uint16_t pid, const char* note) {
        identity_vid_ = vid;
        identity_pid_ = pid;
        if (note) {
            std::strncpy(identity_note_, note, sizeof(identity_note_) - 1);
            identity_note_[sizeof(identity_note_) - 1] = '\0';
        } else {
            identity_note_[0] = '\0';
        }
    }

    UsbSessionTimeline& timeline() { return timeline_; }
    const UsbSessionTimeline& timeline() const { return timeline_; }

    Plane plane() const { return plane_; }
    uint16_t identityVid() const { return identity_vid_; }
    uint16_t identityPid() const { return identity_pid_; }
    const char* identityNote() const { return identity_note_; }
    const char* visibilityScope() const { return visibility_scope_; }
    bool isFullEp0Trace() const { return is_full_ep0_trace_; }

    // --- AOA notes ---
    void noteAttach(uint32_t t_ms) {
        bump(attach_);
        timeline_.push(UsbTlEvent::Attach, t_ms, attach_.value);
    }
    void noteDetach(uint32_t t_ms) {
        bump(detach_);
        timeline_.push(UsbTlEvent::Detach, t_ms, detach_.value);
        if (attach_.vis == MetricVis::Measured && detach_.value > 0 &&
            attach_.value > detach_.value) {
            // reconnect ≈ attach after first detach cycle
            if (reconnect_.isUnknown()) reconnect_ = MetricU32::measured(0);
            // Heuristic: each attach after the first counts as reconnect when detach seen.
            reconnect_.value = attach_.value > 1 ? attach_.value - 1 : 0;
            reconnect_.vis = MetricVis::Measured;
        }
    }
    void noteSoftDetach(uint32_t t_ms) { timeline_.push(UsbTlEvent::SoftDetach, t_ms, 0); }
    void noteSoftAttach(uint32_t t_ms) { timeline_.push(UsbTlEvent::SoftAttach, t_ms, 0); }
    void noteVendorMounted(uint32_t t_ms) {
        timeline_.push(UsbTlEvent::VendorMounted, t_ms, 0);
    }
    void noteSetup(uint32_t t_ms, bool vendor) {
        bump(control_setup_);
        if (vendor) bump(vendor_setup_);
        timeline_.push(UsbTlEvent::Setup, t_ms, vendor ? 1u : 0u);
    }
    void noteAoa51(uint32_t t_ms) {
        bump(aoa51_);
        timeline_.push(UsbTlEvent::Aoa51, t_ms, aoa51_.value);
    }
    void noteAoa52(uint32_t t_ms) {
        bump(aoa52_);
        timeline_.push(UsbTlEvent::Aoa52, t_ms, aoa52_.value);
    }
    void noteAoa53(uint32_t t_ms) {
        bump(aoa53_);
        timeline_.push(UsbTlEvent::Aoa53, t_ms, aoa53_.value);
    }
    void noteAoa51RespondOk() { bump(aoa51_ok_); }
    void noteBulkRx(uint32_t t_ms, uint32_t n) {
        bump(bulk_rx_);
        add(rx_bytes_, n);
        timeline_.push(UsbTlEvent::BulkRx, t_ms, n);
    }
    void noteBulkTx(uint32_t n) {
        bump(bulk_tx_);
        add(tx_bytes_, n);
    }

    // --- MSC notes ---
    void noteScsiOpcode(uint8_t op, uint32_t t_ms) {
        if (!scsi_hist_armed_) return;
        bump(scsi_command_);
        const size_t idx = scsiHistIndex(op);
        ++scsi_hist_[idx];
        timeline_.push(UsbTlEvent::ScsiOpcode, t_ms, op);
    }
    void noteRead(uint32_t bytes) {
        bump(read_count_);
        add(read_bytes_, bytes);
    }
    void noteWrite(uint32_t bytes) {
        bump(write_count_);
        add(write_bytes_, bytes);
    }
    void syncPlugCounts(uint32_t plug, uint32_t unplug) {
        if (attach_.isUnknown()) attach_ = MetricU32::measured(0);
        if (detach_.isUnknown()) detach_ = MetricU32::measured(0);
        attach_.value = plug;
        detach_.value = unplug;
        if (reconnect_.isUnknown()) reconnect_ = MetricU32::measured(0);
        reconnect_.value = plug > 1 ? plug - 1 : 0;
    }

    const MetricU32& attach() const { return attach_; }
    const MetricU32& detach() const { return detach_; }
    const MetricU32& reconnect() const { return reconnect_; }
    const MetricU32& enumeration() const { return enumeration_; }
    const MetricU32& configuration() const { return configuration_; }
    const MetricU32& controlSetup() const { return control_setup_; }
    const MetricU32& controlIn() const { return control_in_; }
    const MetricU32& controlOut() const { return control_out_; }
    const MetricU32& vendorSetup() const { return vendor_setup_; }
    const MetricU32& aoa51() const { return aoa51_; }
    const MetricU32& aoa52() const { return aoa52_; }
    const MetricU32& aoa53() const { return aoa53_; }
    const MetricU32& aoa51Ok() const { return aoa51_ok_; }
    const MetricU32& bulkRx() const { return bulk_rx_; }
    const MetricU32& bulkTx() const { return bulk_tx_; }
    const MetricU32& rxBytes() const { return rx_bytes_; }
    const MetricU32& txBytes() const { return tx_bytes_; }
    const MetricU32& scsiCommand() const { return scsi_command_; }
    const MetricU32& readCount() const { return read_count_; }
    const MetricU32& readBytes() const { return read_bytes_; }
    const MetricU32& writeCount() const { return write_count_; }
    const MetricU32& writeBytes() const { return write_bytes_; }
    const MetricU32& resetCount() const { return reset_count_; }
    const MetricU32& clearHalt() const { return clear_halt_; }

    /** @brief SCSI opcode histogram slots (MSC plane only). */
    enum ScsiHist : size_t {
        kTur = 0,
        kInquiry,
        kModeSense6,
        kPreventAllow,
        kReadCapacity10,
        kRead10,
        kWrite10,
        kOther,
        kHistCount
    };
    uint32_t scsiHist(ScsiHist i) const {
        return (scsi_hist_armed_ && i < kHistCount) ? scsi_hist_[i] : 0;
    }
    bool scsiHistArmed() const { return scsi_hist_armed_; }

    static const char* scsiHistName(ScsiHist i) {
        switch (i) {
            case kTur: return "tur";
            case kInquiry: return "inquiry";
            case kModeSense6: return "mode_sense6";
            case kPreventAllow: return "prevent_allow";
            case kReadCapacity10: return "read_capacity10";
            case kRead10: return "read10";
            case kWrite10: return "write10";
            default: return "other";
        }
    }

private:
    static void bump(MetricU32& m) {
        if (m.isUnknown()) m = MetricU32::measured(0);
        ++m.value;
    }
    static void add(MetricU32& m, uint32_t n) {
        if (m.isUnknown()) m = MetricU32::measured(0);
        m.value += n;
    }

    static size_t scsiHistIndex(uint8_t op) {
        switch (op) {
            case 0x00: return kTur;
            case 0x12: return kInquiry;
            case 0x1A: return kModeSense6;
            case 0x1E: return kPreventAllow;
            case 0x25: return kReadCapacity10;
            case 0x28: return kRead10;
            case 0x2A: return kWrite10;
            default: return kOther;
        }
    }

    Plane plane_ = Plane::None;
    uint16_t identity_vid_ = 0;
    uint16_t identity_pid_ = 0;
    char identity_note_[64] = {0};
    char visibility_scope_[40] = {0};
    bool is_full_ep0_trace_ = false;

    MetricU32 attach_, detach_, reconnect_;
    MetricU32 enumeration_, configuration_;
    MetricU32 control_setup_, control_in_, control_out_, vendor_setup_;
    MetricU32 aoa51_, aoa52_, aoa53_, aoa51_ok_;
    MetricU32 bulk_rx_, bulk_tx_, rx_bytes_, tx_bytes_;
    MetricU32 scsi_command_;
    uint32_t scsi_hist_[kHistCount] = {0};
    bool scsi_hist_armed_ = false;
    MetricU32 read_count_, read_bytes_, write_count_, write_bytes_;
    MetricU32 reset_count_, clear_halt_;
    UsbSessionTimeline timeline_;
};

}  // namespace huprobe
