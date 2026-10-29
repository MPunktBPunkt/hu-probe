#pragma once

/**
 * @file ConfigStore.h
 * @brief Persistent device configuration backed by NVS (Preferences).
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "BuildFlags.h"

/**
 * @brief Device configuration with NVS persistence and JSON (de)serialization.
 *
 * NVS namespace "esphub" holds the device-family fields (name, hub, heartbeat, mDNS),
 * namespace "huprobe" holds the measurement configuration (collector, IDs, NTP, drive mode, OTA).
 */
class ConfigStore {
public:
    String deviceName = DEVICE_NAME_DEFAULT;
    String hubHost = HUB_HOST_DEFAULT;
    int hubPort = HUB_PORT_DEFAULT;
    bool enableHub = true;
    uint16_t heartbeatIntervalS = 30;
    bool enableMdns = true;
    uint16_t watchdogS = 0;

    String collectorHost = COLLECTOR_HOST_DEFAULT;
    int collectorPort = COLLECTOR_PORT_DEFAULT;
    bool enableCollector = true;

    // Lab default: never silently tag dry-runs as BMW.
    String vehicleId = "lab_debian";
    String profileId = "lab_debian";
    String scenarioId = "s0_plug_enum";

    /** Offline timer end: media_hide (default) or soft_detach (legacy). */
    String offlineEndStrategy = "media_hide";

    bool enableNtp = true;
    String ntpServer = NTP_SERVER_DEFAULT;
    String tz = TZ_DEFAULT;

    // Drive mode (0.3.0): capture on every boot, flash journal, upload at home.
    bool autoCapture = false;
    String autoExperiment = "drive_home";
    String autoMedia = "drive_mix";
    bool autoFine = true;

    /** Second WLAN (phone hotspot); the home WLAN stays in the WiFiManager store. */
    String wifi2Ssid;
    String wifi2Pass;
    bool autoUpdate = true;
    String otaManifestUrl = OTA_MANIFEST_URL_DEFAULT;

    /**
     * @brief USB gadget boot mode (C1b-P0 isolation).
     *
     * "msc" = Golden Path USBMSC only (default).
     * "aoa_observe" = USBVendor AOA observe only — no MSC interface.
     * Switch via NVS + reboot only; soft-detach is not a mode change.
     */
    String usbGadget = "msc";

    /** @brief Initialize the store by loading the configuration from NVS (calls load()). */
    void begin();

    /** @brief True when usbGadget == "aoa_observe". */
    bool isAoaObserve() const { return usbGadget == "aoa_observe"; }

    /** @brief True when usbGadget == "msc" (Golden Path). */
    bool isMscGadget() const { return usbGadget != "aoa_observe"; }

    /**
     * @brief Reset all fields to defaults, then overlay the values stored in NVS.
     *
     * Reads namespaces "esphub" and "huprobe" read-only; missing keys keep their default.
     * An unknown offlineEndStrategy is coerced back to "media_hide".
     */
    void load();

    /**
     * @brief Write all fields to NVS namespaces "esphub" and "huprobe".
     *
     * Each namespace also gets the config version key "ver" (kConfigVersion).
     * @warning Writes flash; a namespace that cannot be opened is silently skipped.
     */
    void save();

    /** @brief Clear both NVS namespaces ("esphub", "huprobe") and restore the in-memory defaults. */
    void factoryReset();

    /** @brief Reset all in-memory fields to their compile-time/lab defaults (does not touch NVS). */
    void applyDefaults();

    /**
     * @brief Serialize the configuration plus firmware/board info into a JSON object.
     *
     * The WLAN2 password is never emitted; only "wifi2PassSet" (true if non-empty) is reported.
     * Adds read-only keys "fwVersion", "fwType" and "board".
     *
     * @param obj  Target JSON object to fill.
     */
    void toJson(JsonObject obj) const;

    /**
     * @brief Apply the fields present in a JSON object to the in-memory configuration.
     *
     * Missing or wrongly typed keys leave the field unchanged; an offlineEndStrategy other than
     * "media_hide"/"soft_detach" is ignored; an empty "wifi2Pass" keeps the stored password.
     * Does not persist; call save() afterwards.
     *
     * @param obj  JSON object with camelCase keys as produced by toJson().
     * @return Always true.
     */
    bool fromJson(JsonVariantConst obj);

private:
    static constexpr uint8_t kConfigVersion = 1;
};
