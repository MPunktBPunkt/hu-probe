#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>

/**
 * Prefer PSRAM for large durable buffers (offline capture, EventBus ring slots).
 * Falls back to internal heap when PSRAM is absent or exhausted.
 */
namespace HuMem {

inline void* allocPreferPsram(size_t n) {
    if (n == 0) return nullptr;
#ifdef BOARD_HAS_PSRAM
    if (ESP.getPsramSize() > 0) {
        void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p) return p;
    }
#endif
    return malloc(n);
}

inline void* callocPreferPsram(size_t count, size_t size) {
    const size_t n = count * size;
    if (n == 0) return nullptr;
#ifdef BOARD_HAS_PSRAM
    if (ESP.getPsramSize() > 0) {
        void* p = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p) return p;
    }
#endif
    return calloc(count, size);
}

inline void freePreferPsram(void* p) {
    if (!p) return;
    // heap_caps_free works for both SPIRAM and internal caps allocations;
    // plain malloc also goes through the heap_caps path on Arduino-ESP32.
    heap_caps_free(p);
}

inline bool ptrInPsram(const void* p) {
    if (!p) return false;
#ifdef BOARD_HAS_PSRAM
    return esp_ptr_external_ram(p);
#else
    (void)p;
    return false;
#endif
}

}  // namespace HuMem
