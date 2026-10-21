/**
 * @file GithubOta.cpp
 * @brief Manifest fetch, streamed OTA download with sha256 check and NVS rollback state.
 */
#include "core/GithubOta.h"
#include "BuildFlags.h"
#include "core/DiagLog.h"
#include "core/PanicTail.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

extern const uint8_t ca_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");
extern const uint8_t ca_bundle_end[] asm("_binary_data_cert_x509_crt_bundle_bin_end");

namespace GithubOta {
namespace {

constexpr const char* kNs = "gh_ota";
constexpr uint32_t kManifestTimeoutMs = 10000;
constexpr uint32_t kDownloadMaxMs = 180000;
constexpr uint8_t kMaxRedirects = 5;

String lastResult = "not_checked";
String lastRemote;
String rolledBackFrom;
uint8_t bootTries = 0;
bool pendingImage = false;

/**
 * @brief Issue an HTTPS GET, following up to kMaxRedirects redirects manually.
 *
 * Redirects (301/302/303/307/308) are only followed to https:// locations.
 *
 * @param client     TLS client (CA bundle already set).
 * @param http       HTTP client; left open on success so the caller can read the body.
 * @param url        Initial URL.
 * @param timeoutMs  Connect and read timeout in ms.
 * @param[out] codeOut  Final HTTP status; -1 if begin() failed, -2 if too many redirects,
 *                      or the redirect code if the target was not https.
 * @return true if the final response is 200 (caller must call http.end()).
 */
bool openGet(WiFiClientSecure& client, HTTPClient& http, String url, uint32_t timeoutMs,
             int* codeOut) {
    static const char* hdrs[] = {"Location"};
    for (uint8_t hop = 0; hop <= kMaxRedirects; hop++) {
        http.setTimeout(timeoutMs);
        http.setConnectTimeout(timeoutMs);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        if (!http.begin(client, url)) {
            *codeOut = -1;
            return false;
        }
        http.collectHeaders(hdrs, 1);
        const int code = http.GET();
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            url = http.header("Location");
            http.end();
            if (!url.startsWith("https://")) {
                *codeOut = code;
                return false;
            }
            continue;
        }
        *codeOut = code;
        if (code == 200) return true;
        http.end();
        return false;
    }
    *codeOut = -2;
    return false;
}

/**
 * @brief Store the last check result, copy it to info and log it.
 * @param r          Result string.
 * @param[out] info  Optional (may be null); receives r.
 */
void setResult(const String& r, String* info) {
    lastResult = r;
    if (info) *info = r;
    DiagLog::printf("[GH-OTA] %s\n", r.c_str());
}

}  // namespace

bool caBundleAvailable() { return (ca_bundle_end - ca_bundle_start) > 2; }

int compareVersions(const char* a, const char* b) {
    while (*a || *b) {
        while (*a && !isdigit((unsigned char)*a)) a++;
        while (*b && !isdigit((unsigned char)*b)) b++;
        const long na = *a ? strtol(a, const_cast<char**>(&a), 10) : 0;
        const long nb = *b ? strtol(b, const_cast<char**>(&b), 10) : 0;
        if (na != nb) return na < nb ? -1 : 1;
    }
    return 0;
}

void bootCheck() {
    Preferences p;
    if (!p.begin(kNs, false)) return;
    pendingImage = p.getUChar("pending", 0) != 0;
    if (!pendingImage) {
        p.end();
        return;
    }
    bootTries = p.getUChar("tries", 0) + 1;
    if (bootTries <= kMaxTries) {
        p.putUChar("tries", bootTries);
        p.end();
        return;
    }
    const String prev = p.getString("prev", "");
    p.putUChar("pending", 0);
    p.putUChar("tries", 0);
    const esp_partition_t* target = prev.length()
        ? esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, prev.c_str())
        : nullptr;
    if (!target) {
        p.end();
        return;
    }
    const esp_partition_t* running = esp_ota_get_running_partition();
    p.putString("rolled", running ? running->label : "?");
    p.end();
    if (esp_ota_set_boot_partition(target) == ESP_OK) {
        PanicTail::noteRestart(PanicTail::RestartOtaRollback);
        delay(100);
        ESP.restart();
    }
}

void markHealthy() {
    Preferences p;
    if (!p.begin(kNs, false)) return;
    rolledBackFrom = p.getString("rolled", "");
    if (pendingImage) {
        p.putUChar("pending", 0);
        p.putUChar("tries", 0);
        pendingImage = false;
        DiagLog::printf("[GH-OTA] image %s accepted after %u boot(s)\n", FW_VERSION,
                        (unsigned)bootTries);
    }
    p.end();
}

void check(const String& manifestUrl, String* info) {
    if (WiFi.status() != WL_CONNECTED) return setResult("no_wifi", info);
    if (!manifestUrl.startsWith("https://")) return setResult("no_manifest_url", info);
    if (!caBundleAvailable()) return setResult("no_ca_bundle", info);

    WiFiClientSecure client;
    client.setCACertBundle(ca_bundle_start);
    HTTPClient http;
    int code = 0;
    if (!openGet(client, http, manifestUrl, kManifestTimeoutMs, &code))
        return setResult(String("manifest_http_") + code, info);
    const String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) return setResult("manifest_parse", info);
    const char* version = doc["version"] | "";
    const char* url = doc["url"] | "";
    const char* shaHex = doc["sha256"] | "";
    const size_t size = doc["size"] | 0;
    lastRemote = version;
    if (!(doc["enabled"] | false)) return setResult(String("disabled remote=") + version, info);
    if (!version[0] || compareVersions(version, FW_VERSION) <= 0)
        return setResult(String("up_to_date remote=") + version, info);
    if (strlen(shaHex) != 64 || size == 0 || strncmp(url, "https://", 8) != 0)
        return setResult("manifest_incomplete", info);

    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    if (!next || size > next->size) return setResult("image_too_large", info);

    if (!openGet(client, http, url, kManifestTimeoutMs, &code))
        return setResult(String("download_http_") + code, info);
    if (!Update.begin(size)) {
        http.end();
        return setResult("update_begin", info);
    }
    DiagLog::printf("[GH-OTA] %s -> %s (%u bytes)\n", FW_VERSION, version, (unsigned)size);

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);
    WiFiClient* stream = http.getStreamPtr();
    static uint8_t buf[4096];
    size_t done = 0;
    const unsigned long t0 = millis();
    unsigned long lastData = t0;
    bool failed = false;
    while (done < size) {
        if (millis() - t0 > kDownloadMaxMs || millis() - lastData > 15000) {
            failed = true;
            break;
        }
        const int avail = stream->available();
        if (avail <= 0) {
            if (!stream->connected()) {
                failed = true;
                break;
            }
            delay(5);
            continue;
        }
        const size_t want = min((size_t)avail, min(sizeof(buf), size - done));
        const int n = stream->readBytes(buf, want);
        if (n <= 0) continue;
        mbedtls_sha256_update_ret(&sha, buf, n);
        if (Update.write(buf, n) != (size_t)n) {
            failed = true;
            break;
        }
        done += n;
        lastData = millis();
    }
    http.end();
    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&sha, digest);
    mbedtls_sha256_free(&sha);
    if (failed || done != size) {
        Update.abort();
        return setResult(String("download_failed ") + done + "/" + size, info);
    }
    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (strcasecmp(hex, shaHex) != 0) {
        Update.abort();
        return setResult("sha256_mismatch", info);
    }
    if (!Update.end(true)) return setResult(String("update_end_") + Update.getError(), info);

    const esp_partition_t* running = esp_ota_get_running_partition();
    Preferences p;
    if (p.begin(kNs, false)) {
        p.putUChar("pending", 1);
        p.putUChar("tries", 0);
        p.putString("prev", running ? running->label : "");
        p.putString("ver", version);
        p.remove("rolled");
        p.end();
    }
    setResult(String("installed ") + version + " in " + (millis() - t0) + " ms", info);
    PanicTail::noteRestart(PanicTail::RestartGithubOta);
    delay(200);
    ESP.restart();
}

void toJson(JsonObject obj) {
    obj["result"] = lastResult;
    if (lastRemote.length()) obj["remote"] = lastRemote;
    obj["pending"] = pendingImage;
    obj["boot_tries"] = bootTries;
    obj["ca_bundle"] = caBundleAvailable();
    const esp_partition_t* running = esp_ota_get_running_partition();
    obj["partition"] = running ? running->label : "?";
    if (rolledBackFrom.length()) obj["rolled_back_from"] = rolledBackFrom;
}

}  // namespace GithubOta
