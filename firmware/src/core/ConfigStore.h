#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "BuildFlags.h"

/** NVS "esphub" für Familienfelder, "huprobe" für Mess-Config. */
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

    bool enableNtp = true;
    String ntpServer = NTP_SERVER_DEFAULT;
    String tz = TZ_DEFAULT;

    void begin();
    void load();
    void save();
    void factoryReset();
    void applyDefaults();
    void toJson(JsonObject obj) const;
    bool fromJson(JsonVariantConst obj);

private:
    static constexpr uint8_t kConfigVersion = 1;
};
