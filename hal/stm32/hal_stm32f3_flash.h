#ifndef HAL_STM32F3_FLASH_H
#define HAL_STM32F3_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32F3 flash HAL.
// Reference: RM0316 (STM32F301/F302/F303/F318/F328/F334/F358/F373/F378/F398)
//
// Flash characteristics:
//   - Page size  : 2048 bytes (all F3 devices are high-density equivalent)
//   - Write unit : 2 bytes (half-word)
//   - Erased state: 0xFF
//   - Flash base : 0x08000000
//   - Register layout identical to STM32F1 (KEYR/SR/CR/AR)
//
// flash_base : first address of the flash region for LeafTS
// total_size : bytes for LeafTS — must be a multiple of 2048
// Returns 0 on success, -1 on bad parameters.
int stm32f3_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32F3_FLASH_H
