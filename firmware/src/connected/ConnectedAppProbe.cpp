/**
 * @file ConnectedAppProbe.cpp
 * @brief C1a.5 lab BCL + C1b-0 skeleton + C1b-P0 AOA observe.
 */

#include "connected/ConnectedAppProbe.h"
#include "connected/aoa/AoaControlTap.h"
#include "connected/aoa/AoaObserveGadget.h"
#include "connected/bcl/CaC1LabBclStream.h"
#include "core/DiagLog.h"
#include "core/ProbeClock.h"
#include "net/EventBus.h"

#include "tusb.h"

#include <cstdio>
#include <cstring>
#include <vector>

using huprobe::aoa::AoaControlSample;
using huprobe::aoa::AoaControlTap;
using huprobe::aoa::AoaObserveGadget;
using huprobe::bcl::BclFeedResult;
using huprobe::bcl::BclFrame;
using huprobe::bcl::bclCmdDeskName;
using huprobe::bcl::bclDestDeskLabel;
using huprobe::bcl::buildCaC1LabBclStream;
using huprobe::bcl::kCaC1LabBclChunkCount;
using huprobe::bcl::kCaC1LabBclChunks;
using huprobe::transport::TransportEvent;
using huprobe::transport::transportEventName;

ConnectedAppProbe& ConnectedAppProbe::instance() {
    static ConnectedAppProbe p;
    return p;
}

void ConnectedAppProbe::begin(Session* session, MscGadget* msc, bool aoa_observe_boot,
                              const char* usb_gadget) {
    session_ = session;
    msc_ = msc;
    aoa_observe_boot_ = aoa_observe_boot;
    usb_gadget_ = (usb_gadget && usb_gadget[0]) ? usb_gadget
                                               : (aoa_observe_boot ? "aoa_observe" : "msc");
    state_ = "idle";
    lastErr_ = "";
    DiagLog::printf("[CA] ConnectedAppProbe begin (%s gadget=%s)\n",
                    aoa_observe_boot_ ? "C1b-P0 observe" : "C1a.5/C1b-0 msc boot",
                    usb_gadget_.c_str());
}

void ConnectedAppProbe::bindAoaObserveStack() {
    if (!aoa_observe_boot_) return;
    aoa_.bindObserveStack();
    DiagLog::printf("[CA] AoaTransport bound to observe stack usb_stack_bound=1\n");
}

void ConnectedAppProbe::resetCounters() {
    frames_ = 0;
    malformed_ = 0;
    incomplete_obs_ = 0;
    unknown_cmd_ = 0;
    has_last_ = false;
    last_cmd_ = last_src_ = last_dest_ = last_len_ = 0;
    ca_usb_count_ = 0;
    p0_a_ = p0_b_ = p0_c_ = p0_d_ = p0_e_ = false;
    framer_.reset();
    last_attach_seen_ = 0;
}

void ConnectedAppProbe::publishCaUsb(const char* p0_level, const char* ca_ev, bool hypothesis,
                                     bool direct_obs, JsonDocument& extra) {
    if (!session_ || !session_->active()) return;
    JsonDocument doc;
    const int64_t t = ProbeClock::publishUs(ProbeClock::nowUs());
    doc["v"] = 2;
    doc["sid"] = session_->id();
    doc["seq"] = session_->nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "ca_usb";
    doc["ca_ev"] = ca_ev;
    doc["p0_level"] = p0_level;
    doc["transport"] = "aoa";
    doc["impl"] = aoa_.implLevel();
    doc["usb_stack_bound"] = aoa_.usbStackBound();
    doc["hypothesis"] = hypothesis;
    doc["direct_observation"] = direct_obs;
    doc["evidence_level"] = direct_obs ? "FIELD" : (hypothesis ? "HYPOTHESIS" : "TEST");
    doc["gate"] = "C1b-P0";
    doc["probe_plane"] = "control";
    for (JsonPairConst kv : extra.as<JsonObjectConst>()) {
        doc[kv.key()] = kv.value();
    }
    ca_usb_count_++;
    EventBus::instance().publish(doc);
}

void ConnectedAppProbe::publishTransportEvent(TransportEvent ev, uint32_t n, const char* note) {
    if (!session_ || !session_->active()) return;
    JsonDocument doc;
    const int64_t t = ProbeClock::publishUs(ProbeClock::nowUs());
    doc["v"] = 2;
    doc["sid"] = session_->id();
    doc["seq"] = session_->nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "ca_transport";
    doc["ca_ev"] = transportEventName(ev);
    doc["transport"] = aoa_.id();
    doc["impl"] = aoa_.implLevel();
    doc["usb_role_intent"] = aoa_.usbRoleIntent();
    doc["usb_stack_bound"] = aoa_.usbStackBound();
    doc["armed"] = aoa_.armed();
    doc["connected"] = aoa_.connected();
    doc["rx_bytes"] = aoa_.rxBytes();
    doc["tx_bytes"] = aoa_.txBytes();
    if (n) doc["n"] = n;
    if (note && note[0]) doc["note"] = note;
    doc["gate"] = aoa_.usbStackBound() ? "C1b-P0" : "C1b-0";
    doc["direct_observation"] = aoa_.usbStackBound();
    doc["probe_plane"] = "control";
    EventBus::instance().publish(doc);
}

void ConnectedAppProbe::publishFrame(const BclFrame& f, bool from_transport) {
    if (!session_ || !session_->active()) return;

    frames_++;
    last_cmd_ = f.cmd;
    last_src_ = f.src;
    last_dest_ = f.dest;
    last_len_ = f.len;
    has_last_ = true;
    if (std::strcmp(bclCmdDeskName(f.cmd), "UNKNOWN") == 0) unknown_cmd_++;

    char payload_hex[96];
    payload_hex[0] = '\0';
    {
        static const char* hexd = "0123456789abcdef";
        const size_t n = f.payload.size() < 32 ? f.payload.size() : 32;
        size_t o = 0;
        for (size_t i = 0; i < n && o + 2 < sizeof(payload_hex); ++i) {
            payload_hex[o++] = hexd[(f.payload[i] >> 4) & 0xf];
            payload_hex[o++] = hexd[f.payload[i] & 0xf];
        }
        if (f.payload.size() > 32 && o + 3 < sizeof(payload_hex)) {
            payload_hex[o++] = '.';
            payload_hex[o++] = '.';
            payload_hex[o++] = '.';
        }
        payload_hex[o] = '\0';
    }

    JsonDocument doc;
    const int64_t t = ProbeClock::publishUs(ProbeClock::nowUs());
    doc["v"] = 2;
    doc["sid"] = session_->id();
    doc["seq"] = session_->nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "ca_bcl";
    doc["ca_ev"] = "frame";
    doc["cmd"] = f.cmd;
    doc["cmd_desk_name"] = bclCmdDeskName(f.cmd);
    doc["src"] = f.src;
    doc["dest"] = f.dest;
    if (const char* label = bclDestDeskLabel(f.dest)) {
        doc["dest_desk_label"] = label;
    }
    doc["len"] = f.len;
    doc["payload_hex"] = payload_hex;
    if (from_transport) {
        doc["source"] = aoa_.usbStackBound() ? "transport_aoa_lab_inject" : "transport_aoa_skeleton";
        doc["evidence_level"] = "TEST";
        doc["via"] = "ITransport";
    } else {
        doc["source"] = "synthetic";
        doc["evidence_level"] = "TEST";
    }
    doc["direct_observation"] = false;
    doc["probe_plane"] = "control";
    doc["transport"] = from_transport ? "aoa" : "LAB";
    EventBus::instance().publish(doc);
}

void ConnectedAppProbe::publishError(const BclFeedResult& r, bool from_transport) {
    if (!session_ || !session_->active()) return;

    malformed_++;
    JsonDocument doc;
    const int64_t t = ProbeClock::publishUs(ProbeClock::nowUs());
    doc["v"] = 2;
    doc["sid"] = session_->id();
    doc["seq"] = session_->nextSeq();
    doc["t_us"] = (long long)t;
    doc["t_ms"] = ProbeClock::nowMsCompat(t);
    doc["ev"] = "ca_bcl";
    doc["ca_ev"] = "error";
    doc["error"] = r.error_detail ? r.error_detail : "error";
    doc["error_code"] = static_cast<uint8_t>(r.error_code);
    doc["source"] = from_transport ? "transport_aoa_skeleton" : "synthetic";
    doc["evidence_level"] = "TEST";
    doc["direct_observation"] = false;
    doc["probe_plane"] = "control";
    doc["transport"] = from_transport ? "aoa" : "LAB";
    doc["note"] = "no_resync_DEADBEEF_unsupported";
    EventBus::instance().publish(doc);
}

void ConnectedAppProbe::drainTransportToFramer() {
    uint8_t buf[256];
    for (;;) {
        const size_t n = aoa_.read(buf, sizeof(buf));
        if (n == 0) break;
        publishTransportEvent(TransportEvent::RxBytes, static_cast<uint32_t>(n), "drain");
        BclFeedResult r = framer_.feed(buf, n);
        if (r.incomplete) incomplete_obs_++;
        for (const auto& f : r.frames) publishFrame(f, true);
        if (r.error) {
            publishError(r, true);
            framer_.reset();
        }
    }
}

void ConnectedAppProbe::drainTransportRaw() {
    uint8_t buf[256];
    for (;;) {
        const size_t n = aoa_.read(buf, sizeof(buf));
        if (n == 0) break;
        publishTransportEvent(TransportEvent::RxBytes, static_cast<uint32_t>(n), "bulk_rx");
        if (!p0_e_) {
            p0_e_ = true;
        }
        char hex[96];
        hex[0] = '\0';
        {
            static const char* hexd = "0123456789abcdef";
            const size_t m = n < 32 ? n : 32;
            size_t o = 0;
            for (size_t i = 0; i < m && o + 2 < sizeof(hex); ++i) {
                hex[o++] = hexd[(buf[i] >> 4) & 0xf];
                hex[o++] = hexd[buf[i] & 0xf];
            }
            if (n > 32 && o + 3 < sizeof(hex)) {
                hex[o++] = '.';
                hex[o++] = '.';
                hex[o++] = '.';
            }
            hex[o] = '\0';
        }
        JsonDocument extra;
        extra["n"] = (unsigned)n;
        extra["payload_hex"] = hex;
        extra["note"] = "raw_bulk_no_bcl_framer";
        publishCaUsb("P0-E", "bulk_rx", false, true, extra);
    }
}

void ConnectedAppProbe::drainControlTap() {
    AoaControlSample s;
    while (AoaControlTap::instance().pop(s)) {
        // P0.5-A publishes every callback SETUP; P0_B stays AOA-specific (51/52/53/58).
        const bool aoa_req = (s.b_request == 51 || s.b_request == 52 || s.b_request == 53);
        const bool aoa_or_audio = aoa_req || s.b_request == 58;
        if (!p0_b_ && aoa_or_audio) p0_b_ = true;
        const uint8_t req_type = (s.bm_request_type >> 5) & 0x3;
        JsonDocument extra;
        extra["bm_request_type"] = s.bm_request_type;
        extra["b_request"] = s.b_request;
        extra["w_value"] = s.w_value;
        extra["w_index"] = s.w_index;
        extra["w_length"] = s.w_length;
        extra["stage"] = s.stage;
        extra["stage_name"] = s.stage == CONTROL_STAGE_SETUP   ? "setup"
                              : s.stage == CONTROL_STAGE_DATA  ? "data"
                              : s.stage == CONTROL_STAGE_ACK   ? "ack"
                              : s.stage == CONTROL_STAGE_IDLE  ? "idle"
                                                               : "other";
        extra["request_type"] = req_type == 0   ? "standard"
                                : req_type == 1 ? "class"
                                : req_type == 2 ? "vendor"
                                                : "reserved";
        // device_response_queued: sendResponse() called — not host ACK.
        extra["device_response_queued"] = s.hypothesis_handled;
        extra["hypothesis_handled"] = s.hypothesis_handled;  // legacy alias
        if (aoa_req) {
            extra["aoa_desk_name"] =
                s.b_request == 51 ? "GET_PROTOCOL" : (s.b_request == 52 ? "SEND_STRING" : "START");
        } else if (s.b_request == 58) {
            extra["aoa_desk_name"] = "SET_AUDIO_MODE";
        }
        if (s.str_len && s.str[0]) {
            extra["string_index"] = s.w_index;
            extra["string_utf8"] = s.str;  // audit B3: HU identity strings on req 52
        }
        // Event tag stays aoa_control for schema stability; hypothesis=true only for 51–53/58.
        publishCaUsb("P0-B", "aoa_control", aoa_or_audio, true, extra);
    }
}

void ConnectedAppProbe::loop() {
    if (aoa_observe_boot_) {
        AoaObserveGadget::instance().loop();
    }
    if (!link_observing_) return;
    aoa_.poll();

    if (aoa_observe_boot_) {
        AoaObserveGadget& g = AoaObserveGadget::instance();
        if (g.attachCount() != last_attach_seen_) {
            last_attach_seen_ = g.attachCount();
            if (!p0_a_) {
                p0_a_ = true;
                JsonDocument extra;
                extra["attach_count"] = g.attachCount();
                extra["vid"] = g.vid();
                extra["pid"] = g.pid();
                extra["identity_note"] = g.identityNote();
                publishCaUsb("P0-A", "attach", true, true, extra);
            }
            if (!p0_c_) {
                // Presented identity is hypothesis until host-side capture confirms.
                p0_c_ = true;
                JsonDocument extra;
                extra["vid"] = g.vid();
                extra["pid"] = g.pid();
                extra["note"] = "presented_at_boot_not_post_53_reenum";
                publishCaUsb("P0-C", "vid_pid", true, false, extra);
            }
            if (!p0_d_ && g.vendorMounted()) {
                p0_d_ = true;
                JsonDocument extra;
                extra["note"] = "usbvendor_bulk_pair_arduino_tinyusb";
                extra["vendor_mounted"] = true;
                publishCaUsb("P0-D", "ep", true, true, extra);
            }
        }
        drainControlTap();
        // P0: raw bulk only — no BclFramer (C1b-P1).
        const TransportEvent ev = aoa_.lastEvent();
        if (ev != TransportEvent::None && ev != TransportEvent::RxBytes) {
            publishTransportEvent(ev, 0, aoa_.lastError());
            aoa_.clearLastEvent();
        } else if (ev == TransportEvent::RxBytes) {
            aoa_.clearLastEvent();
        }
        drainTransportRaw();
        return;
    }

    // Skeleton / msc-boot lab path (C1b-0)
    const TransportEvent ev = aoa_.lastEvent();
    if (ev != TransportEvent::None && ev != TransportEvent::RxBytes) {
        publishTransportEvent(ev, 0, aoa_.lastError());
        aoa_.clearLastEvent();
    } else if (ev == TransportEvent::RxBytes) {
        aoa_.clearLastEvent();
    }
    drainTransportToFramer();
}

bool ConnectedAppProbe::runCaC1LabBcl() {
    if (!session_) {
        lastErr_ = "no_session";
        state_ = "blocked";
        return false;
    }
    if (running_ || link_observing_) {
        lastErr_ = "busy";
        return false;
    }
    if (session_->active()) {
        lastErr_ = "session_busy";
        state_ = "blocked";
        return false;
    }

    running_ = true;
    scenario_ = "ca_c1_lab_bcl";
    state_ = "parsing";
    lastErr_ = "";
    resetCounters();

    session_->start("lab", "ca_c1_lab_bcl", "bmw_nbt_evo", nullptr, nullptr, "lab_synthetic");
    last_sid_ = session_->id();
    session_->setPhase("ca_lab_bcl", "synthetic_c1a5");

    const std::vector<uint8_t> stream = buildCaC1LabBclStream();
    size_t off = 0;
    size_t ci = 0;
    bool saw_error = false;

    while (off < stream.size()) {
        size_t n = (ci < kCaC1LabBclChunkCount) ? kCaC1LabBclChunks[ci++] : 1000;
        if (off + n > stream.size()) n = stream.size() - off;
        BclFeedResult r = framer_.feed(stream.data() + off, n);
        if (r.incomplete) incomplete_obs_++;
        for (const auto& f : r.frames) publishFrame(f, false);
        if (r.error) {
            publishError(r, false);
            saw_error = true;
            break;
        }
        off += n;
    }

    session_->stop();
    runs_++;
    running_ = false;

    const bool ok = (frames_ == 3 && malformed_ == 1 && unknown_cmd_ == 1 && saw_error);
    if (ok) {
        state_ = "done";
        lastErr_ = "";
        DiagLog::printf("[CA] ca_c1_lab_bcl PASS frames=%u malformed=%u unknown=%u\n",
                        (unsigned)frames_, (unsigned)malformed_, (unsigned)unknown_cmd_);
    } else {
        state_ = "error";
        lastErr_ = "scenario_mismatch";
        DiagLog::printf("[CA] ca_c1_lab_bcl FAIL frames=%u malformed=%u unknown=%u\n",
                        (unsigned)frames_, (unsigned)malformed_, (unsigned)unknown_cmd_);
    }
    return ok;
}

bool ConnectedAppProbe::startCaC1Link() {
    if (!session_) {
        lastErr_ = "no_session";
        state_ = "blocked";
        return false;
    }
    if (running_ || link_observing_) {
        lastErr_ = "busy";
        return false;
    }
    if (session_->active()) {
        lastErr_ = "session_busy";
        state_ = "blocked";
        return false;
    }

    // P0-0: live AOA observe requires aoa_observe boot — soft-detach is not a mode switch.
    if (!aoa_observe_boot_) {
        lastErr_ = "wrong_usb_gadget";
        state_ = "blocked";
        DiagLog::printf("[CA] ca_c1_link rejected: need usb_gadget=aoa_observe|aoa_phone_like + reboot\n");
        return false;
    }
    if (!aoa_.usbStackBound() || !AoaObserveGadget::instance().ready()) {
        lastErr_ = "aoa_stack_not_ready";
        state_ = "error";
        return false;
    }

    scenario_ = "ca_c1_link";
    resetCounters();
    if (!aoa_.arm()) {
        lastErr_ = aoa_.lastError();
        state_ = "error";
        return false;
    }

    msc_detached_for_link_ = false;  // MSC never present in aoa_observe

    // Lab boots soft-detached (WiFi-first). Attach D+ so the host can see the gadget
    // without Soft-Cycle (Soft-Cycle under OTG often kills WiFi / reboots).
    AoaObserveGadget& g = AoaObserveGadget::instance();
    bool attached_now = false;
    if (!g.softAttached()) {
        attached_now = g.softAttach();
        DiagLog::printf("[CA] link/start softAttach=%d (was detached)\n", attached_now ? 1 : 0);
    }

    session_->start("lab", "ca_c1_link", "bmw_nbt_evo", nullptr, nullptr, usb_gadget_.c_str());
    last_sid_ = session_->id();
    session_->setPhase("ca_link_observe", "c1b_p0_observe");
    link_observing_ = true;
    state_ = "observing";
    lastErr_ = "";
    link_runs_++;

    {
        JsonDocument extra;
        extra["msc_interface_present"] = false;
        extra["vendor_interface_present"] = true;
        extra["usb_gadget"] = usb_gadget_;
        extra["soft_attached"] = g.softAttached();
        extra["soft_attach_on_start"] = attached_now;
        publishCaUsb("P0-0", "gadget_isolation", false, true, extra);
    }
    publishTransportEvent(TransportEvent::Disconnected, 0, "armed_p0_observe_waiting");
    DiagLog::printf("[CA] ca_c1_link armed (C1b-P0 observe, usb_stack_bound=1)\n");
    return true;
}

bool ConnectedAppProbe::stopCaC1Link() {
    if (!link_observing_) {
        lastErr_ = "not_observing";
        return false;
    }
    publishTransportEvent(TransportEvent::Disconnected, 0, "stop");
    aoa_.disarm();
    link_observing_ = false;
    if (session_ && session_->active()) session_->stop();
    state_ = "idle";
    lastErr_ = "";
    DiagLog::printf("[CA] ca_c1_link stopped\n");
    return true;
}

bool ConnectedAppProbe::sendKnock(const char* pattern) {
    if (!link_observing_ || !aoa_.armed()) {
        lastErr_ = "not_observing";
        return false;
    }
    if (!aoa_observe_boot_ || !aoa_.usbStackBound()) {
        lastErr_ = "wrong_usb_gadget";
        return false;
    }
    if (!AoaObserveGadget::instance().vendorMounted()) {
        lastErr_ = "vendor_not_mounted";
        return false;
    }

    const char* name = (pattern && pattern[0]) ? pattern : "hex4";
    uint8_t buf[16];
    size_t len = 0;

    // Desk hypotheses — not NBT wire FACT until observed on this HU.
    if (!strcmp(name, "hex4") || !strcmp(name, "init") || !strcmp(name, "session_init") ||
        !strcmp(name, "12345678")) {
        // BclCarLink / wireshark_bmw_bcl: 4 raw bytes 12 34 56 78 (NOT ASCII).
        buf[0] = 0x12;
        buf[1] = 0x34;
        buf[2] = 0x56;
        buf[3] = 0x78;
        len = 4;
    } else if (!strcmp(name, "ascii8")) {
        // REJECTED lookalike — literal "12345678" (31 32 …). Kept for A/B only.
        static const char kAscii[] = "12345678";
        len = 8;
        std::memcpy(buf, kAscii, len);
    } else if (!strcmp(name, "bcl_knock")) {
        // BCL Knock cmd=8, src/dest/len = 0 (empty) — 8-byte BE header only.
        buf[0] = 0;
        buf[1] = 8;  // Knock
        buf[2] = 0;
        buf[3] = 0;
        buf[4] = 0;
        buf[5] = 0;
        buf[6] = 0;
        buf[7] = 0;
        len = 8;
    } else if (!strcmp(name, "bcl_hs") || !strcmp(name, "bcl_handshake")) {
        buf[0] = 0;
        buf[1] = 4;  // Handshake
        buf[2] = 0;
        buf[3] = 0;
        buf[4] = 0;
        buf[5] = 0;
        buf[6] = 0;
        buf[7] = 0;
        len = 8;
    } else {
        lastErr_ = "unknown_knock_pattern";
        return false;
    }

    aoa_.enableWireTx(true);
    const size_t n = aoa_.write(buf, len);
    aoa_.enableWireTx(false);

    JsonDocument extra;
    extra["pattern"] = name;
    extra["tx_req"] = static_cast<uint32_t>(len);
    extra["tx_ok"] = static_cast<uint32_t>(n);
    extra["gate"] = "B2b";
    char hex[40];
    size_t hp = 0;
    for (size_t i = 0; i < len && hp + 2 < sizeof(hex); ++i) {
        snprintf(hex + hp, sizeof(hex) - hp, "%02x", buf[i]);
        hp += 2;
    }
    hex[hp] = '\0';
    extra["hex"] = hex;
    publishCaUsb("P0-E", "knock_tx", true, true, extra);
    publishTransportEvent(TransportEvent::TxBytes, static_cast<uint32_t>(n), name);

    if (n == 0) {
        lastErr_ = aoa_.lastError();
        if (!lastErr_ || !lastErr_[0]) lastErr_ = "knock_tx_zero";
        DiagLog::printf("[CA] knock FAIL pattern=%s err=%s\n", name, lastErr_);
        return false;
    }
    lastErr_ = "";
    DiagLog::printf("[CA] knock OK pattern=%s n=%u\n", name, (unsigned)n);
    return true;
}

bool ConnectedAppProbe::injectRxHex(const char* hex) {
    if (!link_observing_ || !aoa_.armed()) {
        lastErr_ = "not_observing";
        return false;
    }
    if (!hex || !hex[0]) {
        lastErr_ = "empty_hex";
        return false;
    }
    const size_t hexlen = std::strlen(hex);
    if (hexlen % 2 != 0) {
        lastErr_ = "odd_hex";
        return false;
    }
    std::vector<uint8_t> buf;
    buf.reserve(hexlen / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < hexlen; i += 2) {
        const int hi = nibble(hex[i]);
        const int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            lastErr_ = "bad_hex";
            return false;
        }
        buf.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    const size_t accepted = aoa_.injectRx(buf.data(), buf.size());
    if (accepted != buf.size()) {
        lastErr_ = aoa_.lastError();
        return false;
    }
    if (aoa_.lastEvent() == TransportEvent::Connected) {
        publishTransportEvent(TransportEvent::Connected, 0, "lab_inject");
        aoa_.clearLastEvent();
    }
    // Lab inject may still exercise BclFramer (TEST). Live bulk never does in P0.
    if (feed_framer_on_inject_) {
        drainTransportToFramer();
    } else {
        drainTransportRaw();
    }
    lastErr_ = "";
    return true;
}

bool ConnectedAppProbe::labSetConnected(bool up) {
    if (!link_observing_ || !aoa_.armed()) {
        lastErr_ = "not_observing";
        return false;
    }
    aoa_.setConnectedForLab(up);
    publishTransportEvent(aoa_.lastEvent(), 0, "lab_set_connected");
    aoa_.clearLastEvent();
    lastErr_ = "";
    return true;
}

bool ConnectedAppProbe::setHypothesisRespond(bool on) {
    if (!aoa_observe_boot_) {
        lastErr_ = "need_aoa_observe_boot";
        return false;
    }
    AoaControlTap::instance().setHypothesisRespond(on);
    DiagLog::printf("[CA] hypothesis_respond=%d\n", on ? 1 : 0);
    lastErr_ = "";
    return true;
}

void ConnectedAppProbe::toStatusJson(JsonObject obj) const {
    obj["plane"] = "CONTROL";
    obj["scenario"] = scenario_;
    obj["gate"] = link_observing_ ? (aoa_observe_boot_ ? "C1b-P0" : "C1b-0")
                                  : "C1a.5";
    obj["state"] = state_;
    obj["running"] = running_;
    obj["link_observing"] = link_observing_;
    obj["usb_gadget"] = usb_gadget_;
    obj["msc_interface_present"] = !aoa_observe_boot_;
    obj["vendor_interface_present"] =
        aoa_observe_boot_ && AoaObserveGadget::instance().vendorInterfacePresent();
    if (aoa_observe_boot_) {
        obj["aoa_identity_profile"] =
            AoaObserveGadget::instance().profile() ==
                    huprobe::aoa::AoaIdentityProfile::PhoneLike
                ? "phone_like"
                : "accessory";
    }
    obj["frames"] = frames_;
    obj["malformed"] = malformed_;
    obj["incomplete"] = incomplete_obs_;
    obj["unknown_cmd"] = unknown_cmd_;
    obj["runs"] = runs_;
    obj["link_runs"] = link_runs_;
    obj["ca_usb_count"] = ca_usb_count_;
    if (last_sid_.length()) obj["last_sid"] = last_sid_;
    if (lastErr_ && lastErr_[0]) obj["last_err"] = lastErr_;
    obj["claims"] = aoa_observe_boot_ ? "aoa_observe_p0_usb_only_no_msc"
                                      : (link_observing_ ? "aoa_transport_skeleton_only"
                                                         : "synthetic_parser_device_integration_only");
    obj["not_claimed"] = "nbt_wire_bcl,sas,rhmi,cds,av,aoa_equals_bcl";
    obj["msc_detached_for_link"] = msc_detached_for_link_;

    JsonObject p0 = obj["p0"].to<JsonObject>();
    p0["P0_0"] = aoa_observe_boot_;
    p0["P0_A"] = p0_a_;
    p0["P0_B"] = p0_b_;
    p0["P0_B_aoa_entry_callback_seen"] = p0_b_;
    p0["P0_B_scope"] = "vendor_callback_51_52_53_58";
    p0["P0_C"] = p0_c_;
    p0["P0_D"] = p0_d_;
    p0["P0_E"] = p0_e_;

    JsonObject tr = obj["transport"].to<JsonObject>();
    tr["id"] = aoa_.id();
    tr["impl"] = aoa_.implLevel();
    tr["usb_role_intent"] = aoa_.usbRoleIntent();
    tr["usb_stack_bound"] = aoa_.usbStackBound();
    tr["armed"] = aoa_.armed();
    // connected == usb_function_mounted; not application peer.
    tr["connected"] = aoa_.connected();
    tr["usb_function_mounted"] = aoa_.usbFunctionMounted();
    tr["application_peer_proven"] = aoa_.applicationPeerProven();
    tr["rx_bytes"] = aoa_.rxBytes();
    tr["tx_bytes"] = aoa_.txBytes();
    tr["inject_count"] = aoa_.injectCount();
    tr["rx_overflow"] = aoa_.rxOverflow();
    tr["bulk_rx_events"] = aoa_.bulkRxEvents();
    tr["last_event"] = transportEventName(aoa_.lastEvent());
    if (aoa_.lastError() && aoa_.lastError()[0]) tr["last_error"] = aoa_.lastError();
    tr["c1b_stage"] = aoa_.usbStackBound() ? "P0" : "skeleton";

    if (aoa_observe_boot_) {
        const AoaObserveGadget& g = AoaObserveGadget::instance();
        tr["vid"] = g.vid();
        tr["pid"] = g.pid();
        tr["identity_note"] = g.identityNote();
        tr["vendor_mounted"] = g.vendorMounted();
        tr["attach_count"] = g.attachCount();
        const AoaControlTap& tap = AoaControlTap::instance();
        tr["setup_count"] = tap.setupCount();
        tr["vendor_setup_count"] = tap.vendorSetupCount();
        tr["drops"] = tap.dropCount();
        tr["capture_valid"] = tap.captureValid();
        tr["visibility_scope"] = "usbvendor_onrequest_only";
        tr["is_full_ep0_trace"] = false;
        // Legacy: historically Vendor-SETUPs only (= vendor_setup_count).
        tr["aoa_control_count"] = tap.vendorSetupCount();
        tr["aoa51"] = tap.aoa51Count();
        tr["aoa52"] = tap.aoa52Count();
        tr["aoa53"] = tap.aoa53Count();
        tr["aoa58"] = tap.aoa58Count();
        tr["aoa51_respond_ok"] = tap.aoa51RespondOk();
        tr["hypothesis_respond"] = tap.hypothesisRespond();
        tr["aoa_mode"] = tap.hypothesisRespond() ? "phone_like_responder" : "observe";
    }

    if (has_last_) {
        JsonObject last = obj["last_frame"].to<JsonObject>();
        last["cmd"] = last_cmd_;
        last["cmd_desk_name"] = bclCmdDeskName(last_cmd_);
        last["src"] = last_src_;
        last["dest"] = last_dest_;
        if (const char* label = bclDestDeskLabel(last_dest_)) {
            last["dest_desk_label"] = label;
        }
        last["len"] = last_len_;
    }
}
