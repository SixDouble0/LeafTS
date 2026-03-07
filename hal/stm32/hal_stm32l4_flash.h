#ifndef HAL_STM32L4_FLASH_H
#define HAL_STM32L4_FLASH_H

#include "hal_flash.h"

// Initialize STM32L4 flash HAL.
// flash_base : first address of the flash region LeafTS should use
//              (e.g. 0x08040000 to leave the first 256 KB for firmware)
// total_size : how many bytes LeafTS may use  (must be a multiple of 2048)
// Returns 0 on success, -1 on bad parameters.
int stm32l4_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32L4_FLASH_H
