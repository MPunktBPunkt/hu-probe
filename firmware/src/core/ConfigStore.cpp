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
    vehicleId = "bmw";
    profileId = "bmw_nbt_evo";
    scenarioId = "s0_plug_enum";
    enableNtp = true;
    ntpServer = NTP_SERVER_DEFAULT;
    tz = TZ_DEFAULT;
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
        enableNtp = prefs.getBool("ntp", enableNtp);
        ntpServer = prefs.getString("ntp_srv", ntpServer);
        tz = prefs.getString("tz", tz);
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
        prefs.putBool("ntp", enableNtp);
        prefs.putString("ntp_srv", ntpServer);
        prefs.putString("tz", tz);
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
    obj["enableNtp"] = enableNtp;
    obj["ntpServer"] = ntpServer;
    obj["tz"] = tz;
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
    if (!obj["enableNtp"].isNull()) enableNtp = obj["enableNtp"].as<bool>();
    if (obj["ntpServer"].is<const char*>()) ntpServer = obj["ntpServer"].as<String>();
    if (obj["tz"].is<const char*>()) tz = obj["tz"].as<String>();
    return true;
}
