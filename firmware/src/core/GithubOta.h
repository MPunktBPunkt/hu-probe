#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Firmware update from a GitHub-hosted manifest (works over any internet WLAN, e.g. a
 * phone hotspot that cannot reach the collector).
 *
 * Manifest: {"enabled":bool,"version":"x.y.z","url":"https://…bin","sha256":"hex","size":n}
 * Only `enabled:true` and a strictly newer version trigger a download. The image is
 * streamed into the next OTA slot, its sha256 checked before Update.end(), then restart.
 *
 * Rollback (the Arduino bootloader has no app-rollback): a fresh image is "pending" in
 * NVS; each boot increments a try counter, markHealthy() clears it. After kMaxTries boots
 * without markHealthy() the previous slot is made the boot partition again.
 */
namespace GithubOta {

constexpr uint8_t kMaxTries = 3;

/** Very early in boot (after NVS is usable): count tries, roll back if needed. */
void bootCheck();
/** Boot reached a working state (WLAN handled): the running image is accepted. */
void markHealthy();
/** Check the manifest and install if newer. Returns only if no update was installed. */
void check(const String& manifestUrl, String* info);
/** Compare dotted versions numerically; <0, 0, >0 like strcmp. */
int compareVersions(const char* a, const char* b);
bool caBundleAvailable();
void toJson(JsonObject obj);

}  // namespace GithubOta
