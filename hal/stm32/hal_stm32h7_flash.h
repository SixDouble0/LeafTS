#ifndef HAL_STM32H7_FLASH_H
#define HAL_STM32H7_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32H7 flash HAL.
// Reference: RM0433 (STM32H742/H743/H750/H753/H757/H747)
//
// Flash characteristics:
//   - Sector size  : 131072 bytes (128 KB)
//   - Write unit   : 32 bytes (flash word = 256-bit, eight consecutive 32-bit writes)
//   - Erased state : 0xFF
//   - Flash base   : 0x08000000 (bank 1); 0x08100000 (bank 2 on 2 MB devices)
//   - FLASH_REG_BASE: 0x52002000 (very different from other STM32 families!)
//   - Register layout significantly different from STM32L4/G0/G4
//
// LeafTS must be configured to use only complete 128 KB sectors.
//
// flash_base : start of the LeafTS flash region (must be 128 KB sector-aligned)
// total_size : bytes for LeafTS — must be a multiple of 131072 (128 KB)
// Returns 0 on success, -1 on bad parameters.
int stm32h7_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32H7_FLASH_H
