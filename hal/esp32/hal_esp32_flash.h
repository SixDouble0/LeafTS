#ifndef HAL_ESP32_FLASH_H
#define HAL_ESP32_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize ESP32 flash HAL.
// Compatible with ESP32, ESP32-S2, ESP32-S3, ESP32-C3.
// Reference: ESP-IDF Programming Guide, esp_flash API
//
// Flash characteristics (external W25Q or similar QSPI flash):
//   - Sector size  : 4096 bytes (erase unit — same for all common SPI flash chips)
//   - Page size    : 256 bytes  (program unit)
//   - Erased state : 0xFF
//   - Flash is NOT directly memory-mapped for writes — access via SPI controller
//
// On ESP32, flash access is handled through the SPI0/SPI1 cache controller.
// This implementation uses esp_flash_read / esp_flash_write / esp_flash_erase_region
// from ESP-IDF (available when compiled with ESP_PLATFORM defined).
// When compiled on host (x86) for compile-check only, stub implementations are used.
//
// flash_base : byte offset within flash where LeafTS data begins.
//              e.g. 0x300000 (3 MB offset in a 4 MB flash, leaving 3 MB for firmware+OTA)
// total_size : bytes for LeafTS — must be a multiple of 4096
// Returns 0 on success, -1 on bad parameters.
int esp32_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_ESP32_FLASH_H
