#ifndef HAL_STM32F7_FLASH_H
#define HAL_STM32F7_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32F7 flash HAL.
// Reference: RM0385 (STM32F745/F746/F756/F765/F767/F769/F777/F779)
//
// Flash sector layout for 2 MB single-bank devices (F7x5/F7x6/F7x7/F7x9):
//   Sectors 0-3 : 32 KB each  (0x08000000 .. 0x0801FFFF)
//   Sector  4   : 128 KB      (0x08020000 .. 0x0803FFFF)
//   Sectors 5-11: 256 KB each (0x08040000 onwards)
//
// LeafTS must use only the uniform 256 KB sectors (5+).
//
// flash_base : start of the LeafTS flash region (e.g. 0x08040000 = sector 5)
// total_size : bytes for LeafTS — must be a multiple of 262144 (256 KB)
// Returns 0 on success, -1 on bad parameters.
int stm32f7_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32F7_FLASH_H
