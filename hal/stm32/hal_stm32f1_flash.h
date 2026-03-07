#ifndef HAL_STM32F1_FLASH_H
#define HAL_STM32F1_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize STM32F1 flash HAL.
// Reference: RM0008 (STM32F101/F102/F103/F105/F107)
//
// Flash page sizes depend on device density:
//   Medium-density (≤128 KB total flash) : page_bytes = 1024
//   High-density   (>128 KB total flash)  : page_bytes = 2048
//   XL-density     (>256 KB total flash)  : page_bytes = 2048
//
// flash_base : first address of the flash region LeafTS should use
//              (e.g. 0x08010000 to leave the first 64 KB for firmware)
// total_size : how many bytes LeafTS may use (must be a multiple of page_bytes)
// page_bytes : erase page size — 1024 (medium-density) or 2048 (high/XL-density)
// Returns 0 on success, -1 on bad parameters.
int stm32f1_flash_init(hal_flash_t *flash,
                       uint32_t flash_base, uint32_t total_size,
                       uint32_t page_bytes);

#endif // HAL_STM32F1_FLASH_H
