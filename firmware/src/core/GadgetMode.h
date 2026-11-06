#pragma once

/**
 * @file GadgetMode.h
 * @brief Host-testable usb_gadget mode helpers (no Arduino / NVS deps).
 *
 * Sticky persistence lives in ConfigStore ("hpgad"); this header owns only
 * validation and load-precedence rules so B1 regressions stay reproducible
 * without an ESP32.
 */

#include <cstring>

namespace huprobe {

/** @brief True for known NVS usb_gadget values (incl. C1 identity experiment). */
inline bool gadgetModeValid(const char* g) {
    if (!g || !g[0]) return false;
    return std::strcmp(g, "msc") == 0 || std::strcmp(g, "aoa_observe") == 0 ||
           std::strcmp(g, "aoa_phone_like") == 0 || std::strcmp(g, "aoa_id_303a") == 0;
}

/** @brief Vendor-only AOA/identity boots — never construct USBMSC. */
inline bool gadgetModeIsAoaVendorOnly(const char* g) {
    if (!g) return false;
    return std::strcmp(g, "aoa_observe") == 0 || std::strcmp(g, "aoa_phone_like") == 0 ||
           std::strcmp(g, "aoa_id_303a") == 0;
}

inline bool gadgetModeIsPhoneLike(const char* g) {
    return g && std::strcmp(g, "aoa_phone_like") == 0;
}

inline bool gadgetModeIsId303a(const char* g) {
    return g && std::strcmp(g, "aoa_id_303a") == 0;
}

inline bool gadgetModeIsMsc(const char* g) {
    return g && std::strcmp(g, "msc") == 0;
}

/**
 * @brief Resolve boot mode: dedicated sticky namespace wins over legacy huprobe key.
 * @param sticky  Value from "hpgad"/"mode" (may be nullptr/empty/invalid).
 * @param legacy  Value from "huprobe"/"usb_gadget".
 * @param fallback  Used when neither is valid (default "msc").
 */
inline const char* resolveGadgetMode(const char* sticky, const char* legacy,
                                     const char* fallback = "msc") {
    if (gadgetModeValid(sticky)) return sticky;
    if (gadgetModeValid(legacy)) return legacy;
    return (fallback && fallback[0]) ? fallback : "msc";
}

/** @brief Soft-detach must never be treated as a mode switch (design invariant). */
inline bool softDetachChangesGadgetMode() { return false; }

}  // namespace huprobe
