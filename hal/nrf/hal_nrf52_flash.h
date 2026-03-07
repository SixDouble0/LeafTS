#ifndef HAL_NRF52_FLASH_H
#define HAL_NRF52_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize nRF52 flash HAL (nRF52832 / nRF52840).
// Reference: nRF52840 Product Specification v1.7, §4.3 NVMC
//
// Flash characteristics:
//   - Page size   : 4096 bytes (erase unit)
//   - Write unit  : 4 bytes (32-bit word, 4-byte aligned)
//   - Erased state: 0xFF
//   - Flash base  : 0x00000000 (nRF52 code flash is at address 0)
//
// flash_base : first address of the flash region for LeafTS
//              (e.g. 0x000E0000 — last 128 KB of 1 MB nRF52840 flash,
//               or 0x00060000 — last 128 KB of 512 KB nRF52832 flash)
// total_size : how many bytes LeafTS may use (must be a multiple of 4096)
// Returns 0 on success, -1 on bad parameters.
int nrf52_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_NRF52_FLASH_H
