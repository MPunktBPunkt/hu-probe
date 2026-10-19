#include "AutoMarker.h"
#include "capture/OfflineCapture.h"
#include "media/ProbeMedia.h"
#include "session/Session.h"

namespace {

constexpr uint8_t kKindCbw = 1;
constexpr uint8_t kKindMount = 7;
constexpr uint8_t kMountTurs = 32;
constexpr uint16_t kMinFileReadBlocks = 9;

Session* g_session = nullptr;
bool g_enabled = true;
uint8_t g_turs = 0;
bool g_mountDone = false;
bool g_firstRead = false;
int16_t g_lastNode = -1;

void emit(int64_t tUs, const char* name, const char* note) {
    OfflineCapture& oc = OfflineCapture::instance();
    if (oc.acceptsPending()) {
        oc.appendAutoMarker(tUs, name);
        return;
    }
    if (oc.active()) return;  // replay running: live markers would interleave
    if (g_session && g_session->active()) g_session->marker(name, note ? note : "auto", tUs);
}

void resetMount() {
    g_turs = 0;
    g_mountDone = false;
    g_firstRead = false;
    g_lastNode = -1;
}

}  // namespace

namespace AutoMarker {

void begin(Session* session) {
    g_session = session;
    resetMount();
}

void setEnabled(bool on) { g_enabled = on; }
bool enabled() { return g_enabled; }

void observe(uint8_t kind, int64_t tUs, const uint8_t* raw31) {
    if (!g_enabled) return;
    if (kind == kKindMount) {
        resetMount();
        return;
    }
    if (kind != kKindCbw || !raw31) return;
    const uint8_t* cdb = raw31 + 15;
    const uint8_t op = cdb[0];
    if (op == 0x00) {
        if (!g_mountDone && ++g_turs >= kMountTurs) {
            g_mountDone = true;
            emit(tUs, "mount_done", "auto");
        }
        return;
    }
    if (op != 0x28) return;
    const uint32_t lba = ((uint32_t)cdb[2] << 24) | ((uint32_t)cdb[3] << 16) |
                         ((uint32_t)cdb[4] << 8) | cdb[5];
    const uint16_t blocks = (uint16_t)((cdb[7] << 8) | cdb[8]);
    if (blocks < kMinFileReadBlocks) return;
    int16_t node = -1;
    const MediaLayout::Region r = ProbeMedia::instance().classify(lba, &node, nullptr);
    if (r != MediaLayout::Region::File || node < 0) return;
    if (!g_firstRead) {
        g_firstRead = true;
        g_lastNode = node;
        emit(tUs, "first_read", "auto");
        return;
    }
    if (node != g_lastNode) {
        g_lastNode = node;
        emit(tUs, "next_track_read", "auto");
    }
}

void onMediaPresent(int64_t) {
    if (g_enabled) resetMount();
}

void onMediaHide(int64_t tUs) {
    if (g_enabled) emit(tUs, "media_hide", "auto");
}

}  // namespace AutoMarker
