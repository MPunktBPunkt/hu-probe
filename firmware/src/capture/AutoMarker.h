#pragma once

#include <Arduino.h>

class Session;

/**
 * Protocol-derived markers (0.2.31, R1). Fed from MscBotTap::drain() for live and
 * offline paths alike, so markers carry the Capture-tUs of the triggering command:
 *
 *   mount_done       32nd TUR after USB mount / media present (BMW mount signature)
 *   first_read       first READ10 > 8 blocks into a file after mount
 *   next_track_read  READ10 > 8 blocks into a different file than the previous one
 *   media_hide       LUN hidden by the probe (offline end, media switch)
 *
 * The 8-block limit skips the mount probe read at LBA 24/8 (inside TRACK1 on demo_v1).
 */
namespace AutoMarker {

void begin(Session* session);
void setEnabled(bool on);
bool enabled();

/** Pending kind as in MscBotTap::Kind (1 = CBW, 7 = mount). raw31 = CBW for kind 1. */
void observe(uint8_t kind, int64_t tUs, const uint8_t* raw31);
void onMediaPresent(int64_t tUs);
void onMediaHide(int64_t tUs);

}  // namespace AutoMarker
