#pragma once

/**
 * @file GithubOta.h
 * @brief Firmware OTA update from a GitHub-hosted manifest with boot-count rollback.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Firmware update from a GitHub-hosted manifest (works over any internet WLAN, e.g. a
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

/** @brief Boots allowed for a pending image without markHealthy() before rolling back. */
constexpr uint8_t kMaxTries = 3;

/**
 * @brief Very early in boot (after NVS is usable): count tries, roll back if needed.
 *
 * Only acts if an image is pending. Once the try counter exceeds kMaxTries, the pending flag
 * is cleared, the previous slot (NVS "prev") is set as boot partition and the device restarts.
 * @warning May not return (ESP.restart() on rollback).
 */
void bootCheck();

/**
 * @brief Boot reached a working state (WLAN handled): the running image is accepted.
 *
 * Clears the pending flag and try counter in NVS and loads a previous rollback label for toJson().
 */
void markHealthy();

/**
 * @brief Check the manifest and install if newer. Returns only if no update was installed.
 *
 * Requires WLAN, an https:// manifest URL and the CA bundle. Manifest and image are fetched
 * over TLS (up to 5 https redirects). The download is aborted after 180 s total or 15 s
 * without data. On success the image is marked pending in NVS and the device restarts.
 *
 * @param manifestUrl  https:// URL of the JSON manifest.
 * @param[out] info    Optional (may be null); receives the result string, e.g. "no_wifi",
 *                     "up_to_date remote=x.y.z", "sha256_mismatch".
 * @warning Blocks the caller for the whole download and writes flash.
 */
void check(const String& manifestUrl, String* info);

/**
 * @brief Compare dotted versions numerically; <0, 0, >0 like strcmp.
 *
 * Non-digit characters are skipped; missing components count as 0 ("1.2" == "1.2.0").
 *
 * @param a  First version string.
 * @param b  Second version string.
 * @return -1 if a < b, 0 if equal, 1 if a > b.
 */
int compareVersions(const char* a, const char* b);

/**
 * @brief Report whether the embedded x509 CA bundle is present.
 * @return true if the linked CA bundle is larger than 2 bytes.
 */
bool caBundleAvailable();

/**
 * @brief Serialize the OTA state (last result, remote version, pending, boot tries,
 *        CA bundle, running partition, rollback origin) into a JSON object.
 * @param obj  Target JSON object.
 */
void toJson(JsonObject obj);

}  // namespace GithubOta
