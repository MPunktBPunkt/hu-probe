#pragma once

#include <ArduinoJson.h>

/**
 * Module identification without esptool: eFuse BLK1 (the fields esptool prints as
 * "Embedded PSRAM 8MB (AP_3v3)") plus the JEDEC ID of the flash chip. Works while PSRAM
 * is disabled in the build. Read once in begin(): the flash ID access briefly stalls
 * the caches, which must not happen while USB MSC is serving reads.
 */
namespace HwInfo {

void begin();
/** e.g. "N16R8"; "?" if the flash size could not be determined. */
const char* moduleGuess();
uint32_t psramBytesEfuse();
uint32_t flashBytesReal();
void toJson(JsonObject o);

}  // namespace HwInfo
