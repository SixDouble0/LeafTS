#ifndef HAL_STM32WL_FLASH_H
#define HAL_STM32WL_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32WL flash HAL.
// Reference: RM0453 (STM32WL55/WL54/WLE5/WLE4)
//
// Flash characteristics:
//   - Page size  : 2048 bytes (single bank, up to 128 pages on WL55 = 256 KB total)
//   - Write unit : 8 bytes (double-word) — same as STM32L4
//   - Erased state: 0xFF
//   - Flash base : 0x08000000
//   - FLASH_REG_BASE: 0x58004000  (same as STM32WB — in CPU1 secure peripheral space)
//   - Register bit layout is identical to STM32L4 (same PG/PER/PNB/STRT/LOCK positions)
//   - Single bank only (no BKER)
//
// flash_base : first address of the flash region for LeafTS
//              must be page-aligned (2 KB boundary)
// total_size : bytes for LeafTS — must be a multiple of 2048
// Returns 0 on success, -1 on bad parameters.
int stm32wl_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32WL_FLASH_H
