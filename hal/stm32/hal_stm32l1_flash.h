#ifndef HAL_STM32L1_FLASH_H
#define HAL_STM32L1_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32L1 flash HAL.
// Reference: RM0038 (STM32L100/L151/L152/L162)
//
// Flash characteristics:
//   - Page size  : 256 bytes (erase unit — much smaller than other STM32 families!)
//   - Write unit : 4 bytes (word, 4-byte aligned)
//   - Erased state: 0x00000000  (NOTE: opposite of most STM32 — erased = zeros!)
//   - Flash base : 0x08000000
//   - FLASH_REG_BASE: 0x40023C00
//   - Uses PECR (Program/Erase Control Register) — very different mechanism from F1/L4!
//   - Two-stage unlock: PEKEYR + PRGKEYR (separate from option byte key)
//
// flash_base : first address of the flash region for LeafTS
// total_size : bytes for LeafTS — must be a multiple of 256
// Returns 0 on success, -1 on bad parameters.
int stm32l1_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32L1_FLASH_H
