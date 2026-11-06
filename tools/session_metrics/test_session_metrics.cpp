/**
 * Host-only SessionMetrics / Timeline / UNKNOWN≠0 tests.
 *
 *   make -C tools/session_metrics test
 */
#include "session/SessionMetrics.h"
#include "session/UsbSessionTimeline.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
            ++g_fail;                                                                          \
        }                                                                                      \
    } while (0)

static void test_timeline_ring_overflow() {
    huprobe::UsbSessionTimeline tl;
    tl.beginSession("sess1", 1000);
    CHECK(tl.size() == 1);  // session_start
    for (uint32_t i = 0; i < 80; ++i) {
        tl.push(huprobe::UsbTlEvent::Attach, 1000 + i, i);
    }
    CHECK(tl.size() == huprobe::UsbSessionTimeline::kCapacity);
    CHECK(tl.overflowCount() > 0);
    huprobe::UsbTlEntry e;
    CHECK(tl.at(0, e));
    // Oldest remaining after overflow — not the first attach.
    CHECK(tl.durationMs() == 79);
    tl.reset();
    CHECK(tl.size() == 0);
    CHECK(!tl.active());
}

static void test_timeline_reset_on_new_session() {
    huprobe::UsbSessionTimeline tl;
    tl.beginSession("a", 10);
    tl.push(huprobe::UsbTlEvent::Setup, 20, 0);
    CHECK(tl.size() == 2);
    tl.beginSession("b", 100);
    CHECK(std::strcmp(tl.sessionId(), "b") == 0);
    CHECK(tl.size() == 1);
    CHECK(tl.overflowCount() == 0);
}

static void test_aoa_measured_zero_vs_unknown() {
    huprobe::SessionMetrics m;
    m.reset();
    // Before plane arming everything is UNKNOWN — must not be treated as measured 0.
    CHECK(m.aoa51().isUnknown());
    CHECK(m.scsiCommand().isUnknown());
    CHECK(m.enumeration().isUnknown());

    m.setPlaneAoa();
    CHECK(!m.aoa51().isUnknown());
    CHECK(m.aoa51().value == 0);  // measured absence
    CHECK(m.enumeration().isUnknown());  // still not on this plane
    CHECK(m.scsiCommand().isUnknown());
    CHECK(m.controlIn().isUnknown());
    CHECK(!m.isFullEp0Trace());
    CHECK(std::strcmp(m.visibilityScope(), "usbvendor_onrequest_only") == 0);

    m.timeline().beginSession("lab", 0);
    m.noteAoa51(5);
    m.noteAoa51(6);
    CHECK(m.aoa51().value == 2);
    CHECK(m.aoa52().value == 0);  // measured 0, not UNKNOWN
    CHECK(!m.aoa52().isUnknown());
}

static void test_msc_scsi_histogram() {
    huprobe::SessionMetrics m;
    m.reset();
    m.setPlaneMsc();
    CHECK(m.aoa51().isUnknown());
    CHECK(!m.scsiCommand().isUnknown());
    CHECK(m.scsiHistArmed());

    m.timeline().beginSession("msc1", 0);
    m.noteScsiOpcode(0x00, 1);  // TUR
    m.noteScsiOpcode(0x1A, 2);  // MODE SENSE(6) — field-measured on BMW via analyzer
    m.noteScsiOpcode(0x1E, 3);  // PREVENT/ALLOW
    m.noteScsiOpcode(0x1A, 4);
    m.noteScsiOpcode(0x28, 5);  // READ10
    m.noteScsiOpcode(0x99, 6);  // other

    CHECK(m.scsiCommand().value == 6);
    CHECK(m.scsiHist(huprobe::SessionMetrics::kTur) == 1);
    CHECK(m.scsiHist(huprobe::SessionMetrics::kModeSense6) == 2);
    CHECK(m.scsiHist(huprobe::SessionMetrics::kPreventAllow) == 1);
    CHECK(m.scsiHist(huprobe::SessionMetrics::kRead10) == 1);
    CHECK(m.scsiHist(huprobe::SessionMetrics::kOther) == 1);
}

static void test_malformed_ignored_before_session() {
    huprobe::UsbSessionTimeline tl;
    tl.push(huprobe::UsbTlEvent::Attach, 1, 0);  // no session → ignored
    CHECK(tl.size() == 0);
}

int main() {
    test_timeline_ring_overflow();
    test_timeline_reset_on_new_session();
    test_aoa_measured_zero_vs_unknown();
    test_msc_scsi_histogram();
    test_malformed_ignored_before_session();
    if (g_fail) {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::puts("session_metrics: OK");
    return 0;
}
