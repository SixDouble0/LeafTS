// STM32WB configuration for the universal HAL flash contract tests.
// WB uses 4 KB pages and double-word writes.

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32wb_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32wb_flash_init;
    cfg->base       = 0x08010000UL;  // first 64 KB reserved for firmware
    cfg->size       = 64u * 1024u;   // 64 KB LeafTS region (multiple of 4 KB page)
    cfg->write_unit = 8u;            // double-word (STM32WB minimum write)
    cfg->erase_unit = 4096u;         // 4 KB page
    cfg->bad_base   = 0x07FFFFFFul;  // below STM32 flash start (0x08000000)
}
