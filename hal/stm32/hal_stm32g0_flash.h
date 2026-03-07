#ifndef HAL_STM32G0_FLASH_H
#define HAL_STM32G0_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32G0 flash HAL.
// Reference: RM0444 (STM32G030/G031/G041/G070/G071/G081/G0B0/G0B1/G0C1)
//
// Flash characteristics:
//   - Page size  : 2048 bytes (erase unit) — same as STM32L4
//   - Write unit : 8 bytes (double-word, two 32-bit writes) — same as STM32L4
//   - Erased state: 0xFF
//   - Flash base : 0x08000000
//   - Single-bank only (no BKER in FLASH_CR unlike L4)
//
// flash_base : first address of the flash region LeafTS should use
//              (e.g. 0x08010000 to leave first 64 KB for firmware)
// total_size : how many bytes LeafTS may use (must be a multiple of 2048)
// Returns 0 on success, -1 on bad parameters.
int stm32g0_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_STM32G0_FLASH_H
