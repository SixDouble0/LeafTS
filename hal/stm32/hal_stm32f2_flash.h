#ifndef HAL_STM32F2_FLASH_H
#define HAL_STM32F2_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32F2 flash HAL.
// Reference: RM0033 (STM32F205/F207/F215/F217)
//
// Flash sector layout (identical to STM32F4):
//   Sectors 0-3 : 16 KB each  (0x08000000 .. 0x0800FFFF)
//   Sector  4   : 64 KB       (0x08010000 .. 0x0801FFFF)
//   Sectors 5+  : 128 KB each (0x08020000 onwards)
//
// LeafTS must use only the uniform 128 KB sectors (5+).
//
// flash_base : start of the LeafTS flash region (e.g. 0x08060000 = sector 7)
// total_size : bytes for LeafTS — must be a multiple of 131072 (128 KB)
// Returns 0 on success, -1 on bad parameters.
int stm32f2_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32F2_FLASH_H
