#ifndef HAL_STM32WB_FLASH_H
#define HAL_STM32WB_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32WB flash HAL.
// Reference: RM0434 (STM32WB55/WB35/WB15/WB10/WB1M)
//
// Flash characteristics:
//   - Page size  : 4096 bytes (single bank, up to 256 pages on WB55 = 1 MB)
//   - Write unit : 8 bytes (double-word) — same as STM32L4
//   - Erased state: 0xFF
//   - Flash base : 0x08000000
//   - FLASH_REG_BASE: 0x58004000  (in CPU1 secure peripheral space — different from L4!)
//   - Register bit layout is identical to STM32L4 (same PG/PER/PNB/STRT/LOCK positions)
//   - No BKER (single bank only)
//
// flash_base : first address of the flash region for LeafTS
//              must be page-aligned (4 KB boundary)
// total_size : bytes for LeafTS — must be a multiple of 4096
// Returns 0 on success, -1 on bad parameters.
int stm32wb_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32WB_FLASH_H
