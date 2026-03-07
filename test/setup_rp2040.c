// RP2040 configuration for the universal HAL flash contract tests.
// RP2040 uses 4 KB erase sectors and 256-byte page-program units.
// Flash is XIP-mapped starting at 0x10000000.

#include "hal_flash_contract.h"
#include "../hal/rp/hal_rp2040_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = rp2040_flash_init;
    cfg->base       = 0x10040000UL;  // 256 KB into XIP space — past firmware
    cfg->size       = 4u * 4096u;    // 16 KB LeafTS region (four 4 KB sectors)
    cfg->write_unit = 256u;          // page-program granularity
    cfg->erase_unit = 4096u;         // SFDP sector erase
    cfg->bad_base   = 0x0FFFFFFFul;  // below XIP base (0x10000000)
}
