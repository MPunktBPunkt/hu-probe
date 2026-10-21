#pragma once

/**
 * @file DemoFatImage.h
 * @brief Geometry and symbol of the built-in demo FAT image (demo_fat.bin) linked into flash.
 */
#include <stdint.h>

/** @brief Total size of the demo FAT image in bytes (512 KiB). */
static const uint32_t DEMO_FAT_SIZE = 524288u;

/** @brief Sector size of the demo FAT image in bytes. */
static const uint16_t DEMO_FAT_SECTOR_SIZE = 512;

/** @brief Number of 512-byte sectors in the demo FAT image (DEMO_FAT_SIZE / DEMO_FAT_SECTOR_SIZE). */
static const uint32_t DEMO_FAT_SECTOR_COUNT = 1024u;

/** @brief Raw demo FAT image bytes in .rodata, embedded from src/msc/demo_fat.bin via .incbin in DemoFatImage.S. */
extern const uint8_t DEMO_FAT_IMAGE[];
