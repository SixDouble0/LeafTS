#ifndef HAL_STM32F4_FLASH_H
#define HAL_STM32F4_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32F4 flash HAL.
// Reference: RM0090 (STM32F405/F407/F415/F417/F427/F429/F437/F439)
//
// NOTE: STM32F4 flash sectors have non-uniform sizes:
//   Sectors 0-3 : 16 KB each  (0x08000000 .. 0x0800FFFF)
//   Sector  4   : 64 KB       (0x08010000 .. 0x0801FFFF)
//   Sectors 5+  : 128 KB each (0x08020000 onwards, up to sector 11)
//
// LeafTS must be configured to use only the uniform 128 KB sectors (5+).
// flash_base must be the start address of one of these sectors.
//
// flash_base : start of the LeafTS flash region (e.g. 0x08060000 = sector 7)
// total_size : bytes for LeafTS — must be a multiple of 131072 (128 KB)
// Returns 0 on success, -1 on bad parameters.
int stm32f4_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32F4_FLASH_H
