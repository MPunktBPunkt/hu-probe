#pragma once

/**
 * @file AutoMarker.h
 * @brief Protocol-derived session markers (mount_done, first_read, next_track_read, media_hide).
 */

#include <Arduino.h>

class Session;

/**
 * @brief Derive markers from the host's SCSI traffic instead of operator input.
 *
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

/**
 * @brief Set the live session that receives markers and reset the mount tracking.
 * @param session  Live session for markers outside the offline capture; may be nullptr.
 */
void begin(Session* session);

/**
 * @brief Enable or disable marker generation (enabled by default).
 * @param on  true to generate markers; false makes observe()/onMedia*() no-ops.
 */
void setEnabled(bool on);

/** @brief True if marker generation is enabled. */
bool enabled();

/**
 * @brief Feed one BOT event; may emit mount_done, first_read or next_track_read.
 *
 * A mount (kind 7) resets the tracking. For CBWs, TEST UNIT READY is counted up to the
 * mount signature and READ10 of ≥ 9 blocks inside a file region is mapped to its file node.
 * The marker goes into the offline capture while it accepts records, else to the active
 * live session; none is emitted while an offline replay runs.
 *
 * @param kind   Pending kind as in MscBotTap::Kind (1 = CBW, 7 = mount); other kinds are ignored.
 * @param tUs    Capture time of the event (ProbeClock µs), used as marker time.
 * @param raw31  Raw 31-byte CBW for kind 1 (CDB at offset 15); may be nullptr otherwise.
 */
void observe(uint8_t kind, int64_t tUs, const uint8_t* raw31);

/**
 * @brief Media became present: restart the mount tracking (TUR count, first read, last file).
 * @param tUs  Event time (ProbeClock µs); currently unused.
 */
void onMediaPresent(int64_t tUs);

/**
 * @brief Emit the media_hide marker (LUN hidden by the probe) if enabled.
 * @param tUs  Time of the hide (ProbeClock µs).
 */
void onMediaHide(int64_t tUs);

}  // namespace AutoMarker
