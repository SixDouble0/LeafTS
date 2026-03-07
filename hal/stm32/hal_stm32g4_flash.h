#ifndef HAL_STM32G4_FLASH_H
#define HAL_STM32G4_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32G4 flash HAL.
// Reference: RM0440 (STM32G431/G441/G471/G473/G474/G483/G484/G491/G4A1)
//
// Flash characteristics:
//   - Page size  : 2048 bytes (same as STM32L4)
//   - Write unit : 8 bytes (double-word) — same as STM32L4
//   - Erased state: 0xFF
//   - Flash base : 0x08000000
//   - FLASH_REG_BASE: 0x40022000 (same as L4)
//   - Single-bank for G431/G441 (128 KB); dual-bank for G474/G484 (512 KB)
//   - This implementation uses single-bank mode (no BKER)
//
// flash_base : first address of the flash region for LeafTS
// total_size : bytes for LeafTS — must be a multiple of 2048
// Returns 0 on success, -1 on bad parameters.
int stm32g4_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32G4_FLASH_H
