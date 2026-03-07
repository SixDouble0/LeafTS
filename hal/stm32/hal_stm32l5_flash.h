#ifndef HAL_STM32L5_FLASH_H
#define HAL_STM32L5_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32L5 flash HAL.
// Reference: RM0438 (STM32L552/L562)
//
// Flash characteristics:
//   - Page size  : 4096 bytes (dual-bank: 2 × 128 pages × 4 KB = 1 MB total)
//   - Write unit : 8 bytes (double-word) — same as STM32L4
//   - Erased state: 0xFF
//   - Flash base : bank 1 = 0x08000000, bank 2 = 0x08040000 (512 KB each)
//   - FLASH_REG_BASE: 0x40022000 (same as L4)
//   - TrustZone-aware: non-secure register layout is identical to L4
//
// flash_base : first address of the flash region for LeafTS
//              must be page-aligned (4 KB boundary)
// total_size : bytes for LeafTS — must be a multiple of 4096
// Returns 0 on success, -1 on bad parameters.
int stm32l5_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32L5_FLASH_H
