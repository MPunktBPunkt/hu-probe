#pragma once

/**
 * @file BuildFlags.h
 * @brief Compile-time defaults (firmware identity, network endpoints, WiFi portal, buffers, board identity).
 *
 * Every value except the board identity can be overridden with a -D build flag.
 */

/** @brief Firmware version string and firmware type tag reported in heartbeats and session events. */
#ifndef FW_VERSION
#define FW_VERSION "0.2.31"
#endif

#ifndef FW_TYPE
#define FW_TYPE "hu-probe"
#endif

/** @brief Default device name (WiFi hostname, heartbeat "name"). */
#ifndef DEVICE_NAME_DEFAULT
#define DEVICE_NAME_DEFAULT "HuProbe"
#endif

/** @brief Default hub host and HTTP port for heartbeats. */
#ifndef HUB_HOST_DEFAULT
#define HUB_HOST_DEFAULT "192.168.178.113"
#endif

#ifndef HUB_PORT_DEFAULT
#define HUB_PORT_DEFAULT 8093
#endif

/** @brief Default collector host and TCP port for the JSONL event stream. */
#ifndef COLLECTOR_HOST_DEFAULT
#define COLLECTOR_HOST_DEFAULT "192.168.178.187"
#endif

#ifndef COLLECTOR_PORT_DEFAULT
#define COLLECTOR_PORT_DEFAULT 9100
#endif

/** @brief WiFiManager setup AP name and config portal timeout in seconds. */
#ifndef WIFI_AP_NAME
#define WIFI_AP_NAME "ESP-HuProbe-Setup"
#endif

#ifndef WIFI_PORTAL_TIMEOUT_S
#define WIFI_PORTAL_TIMEOUT_S 180
#endif

/** @brief Reset/BOOT button GPIO (active low, held at boot forces the WiFi portal in drive mode) and hold time in seconds (RESET_HOLD_SEC is currently not referenced in src/). */
#ifndef RESET_BUTTON_PIN
#define RESET_BUTTON_PIN 0
#endif

#ifndef RESET_HOLD_SEC
#define RESET_HOLD_SEC 3
#endif

/** @brief EventBus ring capacity in JSONL lines. */
#ifndef EVENT_RING_SIZE
#define EVENT_RING_SIZE 2048
#endif

/** @brief Status event interval in ms. Status cadence — 1s flooded lab JSONL; 2.5s keeps telemetry without drowning reads. */
#ifndef STATUS_INTERVAL_MS
#define STATUS_INTERVAL_MS 2500
#endif

/** @brief Default NTP server. */
#ifndef NTP_SERVER_DEFAULT
#define NTP_SERVER_DEFAULT "pool.ntp.org"
#endif

/** @brief Default GitHub OTA manifest URL. */
#ifndef OTA_MANIFEST_URL_DEFAULT
#define OTA_MANIFEST_URL_DEFAULT \
    "https://raw.githubusercontent.com/MPunktBPunkt/hu-probe/main/ota/manifest.json"
#endif

/** @brief Default POSIX TZ string (Central European time with DST). */
#ifndef TZ_DEFAULT
#define TZ_DEFAULT "CET-1CEST,M3.5.0,M10.5.0/3"
#endif

/** @brief Board identity (id, label, hardware type) derived from the IDF target; not overridable. */
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define HUPROBE_BOARD_ID "esp32-s3"
#define HUPROBE_BOARD_LABEL "ESP32-S3"
#define HUPROBE_HW_TYPE "esp32s3"
#else
#define HUPROBE_BOARD_ID "esp32"
#define HUPROBE_BOARD_LABEL "ESP32"
#define HUPROBE_HW_TYPE "esp32"
#endif
