// STM32F3 configuration for the universal HAL flash contract tests.
// F3 uses 2 KB pages and half-word writes.

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32f3_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32f3_flash_init;
    cfg->base       = 0x08010000UL;  // first 64 KB reserved for firmware
    cfg->size       = 64u * 1024u;   // 64 KB LeafTS region (multiple of 2 KB page)
    cfg->write_unit = 2u;            // half-word (STM32F3 minimum write)
    cfg->erase_unit = 2048u;         // 2 KB page
    cfg->bad_base   = 0x07FFFFFFul;  // below STM32 flash start (0x08000000)
}
