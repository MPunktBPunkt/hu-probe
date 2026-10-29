/**
 * @file ConfigStore.cpp
 * @brief NVS load/save/reset and JSON mapping for ConfigStore.
 */
#include "ConfigStore.h"
#include <Preferences.h>

static Preferences prefs;

void ConfigStore::applyDefaults() {
    deviceName = DEVICE_NAME_DEFAULT;
    hubHost = HUB_HOST_DEFAULT;
    hubPort = HUB_PORT_DEFAULT;
    enableHub = true;
    heartbeatIntervalS = 30;
    enableMdns = true;
    watchdogS = 0;
    collectorHost = COLLECTOR_HOST_DEFAULT;
    collectorPort = COLLECTOR_PORT_DEFAULT;
    enableCollector = true;
    vehicleId = "lab_debian";
    profileId = "lab_debian";
    scenarioId = "s0_plug_enum";
    offlineEndStrategy = "media_hide";
    enableNtp = true;
    ntpServer = NTP_SERVER_DEFAULT;
    tz = TZ_DEFAULT;
    autoCapture = false;
    autoExperiment = "drive_home";
    autoMedia = "drive_mix";
    autoFine = true;
    wifi2Ssid = "";
    wifi2Pass = "";
    autoUpdate = true;
    otaManifestUrl = OTA_MANIFEST_URL_DEFAULT;
    usbGadget = "msc";
}

void ConfigStore::begin() {
    load();
}

void ConfigStore::load() {
    applyDefaults();
    if (prefs.begin("esphub", true)) {
        deviceName = prefs.getString("name", deviceName);
        hubHost = prefs.getString("hub_host", hubHost);
        hubPort = prefs.getInt("hub_port", hubPort);
        enableHub = prefs.getBool("enable_hub", enableHub);
        heartbeatIntervalS = prefs.getUShort("hb_s", heartbeatIntervalS);
        enableMdns = prefs.getBool("mdns", enableMdns);
        prefs.end();
    }
    if (prefs.begin("huprobe", true)) {
        collectorHost = prefs.getString("col_host", collectorHost);
        collectorPort = prefs.getInt("col_port", collectorPort);
        enableCollector = prefs.getBool("col_en", enableCollector);
        vehicleId = prefs.getString("vehicle", vehicleId);
        profileId = prefs.getString("profile", profileId);
        scenarioId = prefs.getString("scenario", scenarioId);
        offlineEndStrategy = prefs.getString("off_end", offlineEndStrategy);
        if (offlineEndStrategy != "media_hide" && offlineEndStrategy != "soft_detach") {
            offlineEndStrategy = "media_hide";
        }
        enableNtp = prefs.getBool("ntp", enableNtp);
        ntpServer = prefs.getString("ntp_srv", ntpServer);
        tz = prefs.getString("tz", tz);
        autoCapture = prefs.getBool("auto_cap", autoCapture);
        autoExperiment = prefs.getString("auto_exp", autoExperiment);
        autoMedia = prefs.getString("auto_media", autoMedia);
        autoFine = prefs.getBool("auto_fine", autoFine);
        wifi2Ssid = prefs.getString("wifi2_ssid", wifi2Ssid);
        wifi2Pass = prefs.getString("wifi2_pass", wifi2Pass);
        autoUpdate = prefs.getBool("auto_upd", autoUpdate);
        otaManifestUrl = prefs.getString("ota_url", otaManifestUrl);
        usbGadget = prefs.getString("usb_gadget", usbGadget);
        if (usbGadget != "msc" && usbGadget != "aoa_observe") {
            usbGadget = "msc";
        }
        prefs.end();
    }
}

void ConfigStore::save() {
    if (prefs.begin("esphub", false)) {
        prefs.putUChar("ver", kConfigVersion);
        prefs.putString("name", deviceName);
        prefs.putString("hub_host", hubHost);
        prefs.putInt("hub_port", hubPort);
        prefs.putBool("enable_hub", enableHub);
        prefs.putUShort("hb_s", heartbeatIntervalS);
        prefs.putBool("mdns", enableMdns);
        prefs.end();
    }
    if (prefs.begin("huprobe", false)) {
        prefs.putUChar("ver", kConfigVersion);
        prefs.putString("col_host", collectorHost);
        prefs.putInt("col_port", collectorPort);
        prefs.putBool("col_en", enableCollector);
        prefs.putString("vehicle", vehicleId);
        prefs.putString("profile", profileId);
        prefs.putString("scenario", scenarioId);
        prefs.putString("off_end", offlineEndStrategy);
        prefs.putBool("ntp", enableNtp);
        prefs.putString("ntp_srv", ntpServer);
        prefs.putString("tz", tz);
        prefs.putBool("auto_cap", autoCapture);
        prefs.putString("auto_exp", autoExperiment);
        prefs.putString("auto_media", autoMedia);
        prefs.putBool("auto_fine", autoFine);
        prefs.putString("wifi2_ssid", wifi2Ssid);
        prefs.putString("wifi2_pass", wifi2Pass);
        prefs.putBool("auto_upd", autoUpdate);
        prefs.putString("ota_url", otaManifestUrl);
        prefs.putString("usb_gadget", usbGadget);
        prefs.end();
    }
}

void ConfigStore::factoryReset() {
    if (prefs.begin("esphub", false)) {
        prefs.clear();
        prefs.end();
    }
    if (prefs.begin("huprobe", false)) {
        prefs.clear();
        prefs.end();
    }
    applyDefaults();
}

void ConfigStore::toJson(JsonObject obj) const {
    obj["deviceName"] = deviceName;
    obj["hubHost"] = hubHost;
    obj["hubPort"] = hubPort;
    obj["enableHub"] = enableHub;
    obj["heartbeatIntervalS"] = heartbeatIntervalS;
    obj["enableMdns"] = enableMdns;
    obj["collectorHost"] = collectorHost;
    obj["collectorPort"] = collectorPort;
    obj["enableCollector"] = enableCollector;
    obj["vehicleId"] = vehicleId;
    obj["profileId"] = profileId;
    obj["scenarioId"] = scenarioId;
    obj["offlineEndStrategy"] = offlineEndStrategy;
    obj["enableNtp"] = enableNtp;
    obj["ntpServer"] = ntpServer;
    obj["tz"] = tz;
    obj["autoCapture"] = autoCapture;
    obj["autoExperiment"] = autoExperiment;
    obj["autoMedia"] = autoMedia;
    obj["autoFine"] = autoFine;
    obj["wifi2Ssid"] = wifi2Ssid;
    obj["wifi2PassSet"] = wifi2Pass.length() > 0;
    obj["autoUpdate"] = autoUpdate;
    obj["otaManifestUrl"] = otaManifestUrl;
    obj["usbGadget"] = usbGadget;
    obj["fwVersion"] = FW_VERSION;
    obj["fwType"] = FW_TYPE;
    obj["board"] = HUPROBE_BOARD_LABEL;
}

bool ConfigStore::fromJson(JsonVariantConst obj) {
    if (obj["deviceName"].is<const char*>()) deviceName = obj["deviceName"].as<String>();
    if (obj["hubHost"].is<const char*>()) hubHost = obj["hubHost"].as<String>();
    if (!obj["hubPort"].isNull()) hubPort = obj["hubPort"].as<int>();
    if (!obj["enableHub"].isNull()) enableHub = obj["enableHub"].as<bool>();
    if (!obj["heartbeatIntervalS"].isNull()) heartbeatIntervalS = obj["heartbeatIntervalS"].as<uint16_t>();
    if (!obj["enableMdns"].isNull()) enableMdns = obj["enableMdns"].as<bool>();
    if (obj["collectorHost"].is<const char*>()) collectorHost = obj["collectorHost"].as<String>();
    if (!obj["collectorPort"].isNull()) collectorPort = obj["collectorPort"].as<int>();
    if (!obj["enableCollector"].isNull()) enableCollector = obj["enableCollector"].as<bool>();
    if (obj["vehicleId"].is<const char*>()) vehicleId = obj["vehicleId"].as<String>();
    if (obj["profileId"].is<const char*>()) profileId = obj["profileId"].as<String>();
    if (obj["scenarioId"].is<const char*>()) scenarioId = obj["scenarioId"].as<String>();
    if (obj["offlineEndStrategy"].is<const char*>()) {
        String strategy = obj["offlineEndStrategy"].as<String>();
        if (strategy == "media_hide" || strategy == "soft_detach") {
            offlineEndStrategy = strategy;
        }
    }
    if (!obj["enableNtp"].isNull()) enableNtp = obj["enableNtp"].as<bool>();
    if (obj["ntpServer"].is<const char*>()) ntpServer = obj["ntpServer"].as<String>();
    if (obj["tz"].is<const char*>()) tz = obj["tz"].as<String>();
    if (!obj["autoCapture"].isNull()) autoCapture = obj["autoCapture"].as<bool>();
    if (obj["autoExperiment"].is<const char*>()) autoExperiment = obj["autoExperiment"].as<String>();
    if (obj["autoMedia"].is<const char*>()) autoMedia = obj["autoMedia"].as<String>();
    if (!obj["autoFine"].isNull()) autoFine = obj["autoFine"].as<bool>();
    if (obj["wifi2Ssid"].is<const char*>()) wifi2Ssid = obj["wifi2Ssid"].as<String>();
    // Empty or missing keeps the stored password (it is never sent back to the UI).
    if (obj["wifi2Pass"].is<const char*>() && obj["wifi2Pass"].as<String>().length())
        wifi2Pass = obj["wifi2Pass"].as<String>();
    if (!obj["autoUpdate"].isNull()) autoUpdate = obj["autoUpdate"].as<bool>();
    if (obj["otaManifestUrl"].is<const char*>()) otaManifestUrl = obj["otaManifestUrl"].as<String>();
    if (obj["usbGadget"].is<const char*>()) {
        String g = obj["usbGadget"].as<String>();
        if (g == "msc" || g == "aoa_observe") usbGadget = g;
    }
    return true;
}
