#pragma once

#ifndef FW_VERSION
#define FW_VERSION "0.2.13"
#endif

#ifndef FW_TYPE
#define FW_TYPE "hu-probe"
#endif

#ifndef DEVICE_NAME_DEFAULT
#define DEVICE_NAME_DEFAULT "HuProbe"
#endif

#ifndef HUB_HOST_DEFAULT
#define HUB_HOST_DEFAULT "192.168.178.113"
#endif

#ifndef HUB_PORT_DEFAULT
#define HUB_PORT_DEFAULT 8093
#endif

#ifndef COLLECTOR_HOST_DEFAULT
#define COLLECTOR_HOST_DEFAULT "192.168.178.187"
#endif

#ifndef COLLECTOR_PORT_DEFAULT
#define COLLECTOR_PORT_DEFAULT 9100
#endif

#ifndef WIFI_AP_NAME
#define WIFI_AP_NAME "ESP-HuProbe-Setup"
#endif

#ifndef WIFI_PORTAL_TIMEOUT_S
#define WIFI_PORTAL_TIMEOUT_S 180
#endif

#ifndef RESET_BUTTON_PIN
#define RESET_BUTTON_PIN 0
#endif

#ifndef RESET_HOLD_SEC
#define RESET_HOLD_SEC 3
#endif

#ifndef EVENT_RING_SIZE
#define EVENT_RING_SIZE 2048
#endif

/** Status cadence — 1s flooded lab JSONL; 2.5s keeps telemetry without drowning reads. */
#ifndef STATUS_INTERVAL_MS
#define STATUS_INTERVAL_MS 2500
#endif

#ifndef NTP_SERVER_DEFAULT
#define NTP_SERVER_DEFAULT "pool.ntp.org"
#endif

#ifndef TZ_DEFAULT
#define TZ_DEFAULT "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define HUPROBE_BOARD_ID "esp32-s3"
#define HUPROBE_BOARD_LABEL "ESP32-S3"
#define HUPROBE_HW_TYPE "esp32s3"
#else
#define HUPROBE_BOARD_ID "esp32"
#define HUPROBE_BOARD_LABEL "ESP32"
#define HUPROBE_HW_TYPE "esp32"
#endif
