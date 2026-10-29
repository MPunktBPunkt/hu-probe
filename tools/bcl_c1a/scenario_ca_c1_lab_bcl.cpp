/**
 * @file scenario_ca_c1_lab_bcl.cpp
 * @brief Deterministic host scenario: synthetic BCL stream → JSONL ca_* events.
 *
 * Scenario id: ca_c1_lab_bcl
 * Transport: LAB (no USB-AOA / BT / NBT)
 *
 * Evidence for all bytes:
 *   source = synthetic
 *   evidence_level = TEST
 *   direct_observation = false
 *
 * Host adapter — stream bytes shared with device via CaC1LabBclStream.h.
 */

#include "connected/bcl/BclFramer.h"
#include "connected/bcl/BclTypes.h"
#include "connected/bcl/CaC1LabBclStream.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace huprobe::bcl;

namespace {

struct Status {
    const char* plane = "CONTROL";
    const char* transport = "LAB";
    const char* scenario = "ca_c1_lab_bcl";
    const char* state = "IDLE";
    uint32_t frames = 0;
    uint32_t malformed = 0;
    uint32_t incomplete_observations = 0;
    uint32_t unknown_cmd = 0;
    uint16_t last_cmd = 0;
    uint16_t last_src = 0;
    uint16_t last_dest = 0;
    uint16_t last_len = 0;
    bool has_last = false;
};

uint32_t g_seq = 0;
uint64_t g_t_us = 0;

void emit_line(const std::string& json) {
    std::fwrite(json.data(), 1, json.size(), stdout);
    std::fputc('\n', stdout);
}

std::string hex_of(const std::vector<uint8_t>& v, size_t max_bytes = 32) {
    static const char* hexd = "0123456789abcdef";
    std::string s;
    const size_t n = v.size() < max_bytes ? v.size() : max_bytes;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s.push_back(hexd[(v[i] >> 4) & 0xf]);
        s.push_back(hexd[v[i] & 0xf]);
    }
    if (v.size() > max_bytes) s += "...";
    return s;
}

void emit_session(const char* action, const char* sid) {
    ++g_seq;
    g_t_us += 1000;
    char buf[512];
    std::snprintf(
        buf, sizeof(buf),
        "{\"v\":2,\"sid\":\"%s\",\"seq\":%u,\"t_us\":%llu,\"ev\":\"session\","
        "\"action\":\"%s\",\"vehicle\":\"lab\",\"scenario\":\"ca_c1_lab_bcl\","
        "\"profile_id\":\"bmw_nbt_evo\","
        "\"source\":\"synthetic\",\"evidence_level\":\"TEST\","
        "\"direct_observation\":false,\"probe_plane\":\"control\",\"transport\":\"LAB\"}",
        sid, g_seq, static_cast<unsigned long long>(g_t_us), action);
    emit_line(buf);
}

void emit_frame(const char* sid, const BclFrame& f, Status& st) {
    ++g_seq;
    g_t_us += 1000;
    st.frames++;
    st.last_cmd = f.cmd;
    st.last_src = f.src;
    st.last_dest = f.dest;
    st.last_len = f.len;
    st.has_last = true;
    if (std::strcmp(bclCmdDeskName(f.cmd), "UNKNOWN") == 0) st.unknown_cmd++;

    const char* dest_label = bclDestDeskLabel(f.dest);
    char dest_field[96];
    if (dest_label) {
        std::snprintf(dest_field, sizeof(dest_field),
                      "\"dest\":%u,\"dest_desk_label\":\"%s\"", f.dest, dest_label);
    } else {
        std::snprintf(dest_field, sizeof(dest_field), "\"dest\":%u", f.dest);
    }

    char buf[1024];
    std::snprintf(
        buf, sizeof(buf),
        "{\"v\":2,\"sid\":\"%s\",\"seq\":%u,\"t_us\":%llu,\"ev\":\"ca_bcl\","
        "\"ca_ev\":\"frame\",\"cmd\":%u,\"cmd_desk_name\":\"%s\","
        "\"src\":%u,%s,\"len\":%u,\"payload_hex\":\"%s\","
        "\"source\":\"synthetic\",\"evidence_level\":\"TEST\","
        "\"direct_observation\":false}",
        sid, g_seq, static_cast<unsigned long long>(g_t_us), f.cmd, bclCmdDeskName(f.cmd),
        f.src, dest_field, f.len, hex_of(f.payload).c_str());
    emit_line(buf);
}

void emit_error(const char* sid, const BclFeedResult& r, Status& st) {
    ++g_seq;
    g_t_us += 1000;
    st.malformed++;
    char buf[512];
    std::snprintf(
        buf, sizeof(buf),
        "{\"v\":2,\"sid\":\"%s\",\"seq\":%u,\"t_us\":%llu,\"ev\":\"ca_bcl\","
        "\"ca_ev\":\"error\",\"error\":\"%s\",\"error_code\":%u,"
        "\"source\":\"synthetic\",\"evidence_level\":\"TEST\","
        "\"direct_observation\":false,"
        "\"note\":\"no_resync_DEADBEEF_unsupported\"}",
        sid, g_seq, static_cast<unsigned long long>(g_t_us), r.error_detail,
        static_cast<unsigned>(r.error_code));
    emit_line(buf);
}

void print_status_banner(const Status& st) {
    std::fprintf(stderr,
                 "Connected App Probe (C1a host)\n"
                 "------------------------------\n"
                 "Plane: %s\n"
                 "Transport: %s\n"
                 "Scenario: %s\n"
                 "State: %s\n"
                 "\n"
                 "Frames: %u\n"
                 "Malformed: %u\n"
                 "Incomplete (observed mid-stream): %u\n"
                 "Unknown cmd: %u\n",
                 st.plane, st.transport, st.scenario, st.state, st.frames, st.malformed,
                 st.incomplete_observations, st.unknown_cmd);
    if (st.has_last) {
        const char* dl = bclDestDeskLabel(st.last_dest);
        std::fprintf(stderr,
                     "\nLast frame:\n"
                     "  CMD:  %u (%s)\n"
                     "  SRC:  %u\n"
                     "  DEST: 0x%04X%s%s\n"
                     "  LEN:  %u\n",
                     st.last_cmd, bclCmdDeskName(st.last_cmd), st.last_src, st.last_dest,
                     dl ? " desk_label=" : "", dl ? dl : "", st.last_len);
    }
    std::fprintf(stderr,
                 "\nClaims: parser mechanics on synthetic bytes only.\n"
                 "NOT claimed: NBT Evo wire, AOA, BT, Etch session, SAS, RHMI, CDS, AV.\n");
}

}  // namespace

int main() {
    const char* sid = "LAB-ca_c1_lab_bcl";
    Status st;
    st.state = "PARSING";

    emit_session("start", sid);

    const auto stream = buildCaC1LabBclStream();

    size_t off = 0;
    size_t ci = 0;
    BclFramer fr;
    while (off < stream.size()) {
        size_t n = (ci < kCaC1LabBclChunkCount) ? kCaC1LabBclChunks[ci++] : 1000;
        if (off + n > stream.size()) n = stream.size() - off;
        auto r = fr.feed(stream.data() + off, n);
        if (r.incomplete) st.incomplete_observations++;
        for (const auto& f : r.frames) emit_frame(sid, f, st);
        if (r.error) {
            emit_error(sid, r, st);
            st.state = "ERROR";
            break;
        }
        off += n;
    }

    if (!fr.hasError() && std::strcmp(st.state, "PARSING") == 0) st.state = "DONE";

    emit_session("end", sid);
    print_status_banner(st);

    const bool ok = (st.frames == 3 && st.malformed == 1 && st.unknown_cmd == 1);
    if (!ok) {
        std::fprintf(stderr, "scenario self-check FAIL frames=%u malformed=%u unknown=%u\n",
                     st.frames, st.malformed, st.unknown_cmd);
        return 1;
    }
    std::fprintf(stderr, "scenario self-check PASS\n");
    return 0;
}
