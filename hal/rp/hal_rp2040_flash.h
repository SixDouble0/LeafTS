#ifndef HAL_RP2040_FLASH_H
#define HAL_RP2040_FLASH_H

#include "hal_flash.h"
#include <stdint.h>

// Initialize RP2040 (Raspberry Pi Pico) flash HAL.
// Compatible with RP2350 (Raspberry Pi Pico 2) — same external QSPI flash interface.
// Reference: RP2040 Datasheet §2.8, RP2350 Datasheet §12.8
//
// Flash characteristics:
//   - Sector size  : 4096 bytes (erase unit, via ROM flash_range_erase)
//   - Page size    : 256 bytes  (program unit, via ROM flash_range_program)
//   - Erased state : 0xFF
//   - Flash is XIP-mapped at XIP_BASE = 0x10000000 (256 MB window)
//   - ROM functions handle cache disable/enable automatically
//
// flash_base : byte offset within the flash chip for LeafTS data.
//              e.g. 0x00100000 = 1 MB offset (leave 1 MB for firmware).
//              NOTE: this is an offset, not an XIP address.
// total_size : bytes for LeafTS — must be a multiple of 4096
// Returns 0 on success, -1 on bad parameters.
int rp2040_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size);

#endif // HAL_RP2040_FLASH_H
