// STM32F4 configuration for the universal HAL flash contract tests.
// F4 uses 128 KB sectors (sectors 5-11 are uniform).

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32f4_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32f4_flash_init;
    cfg->base       = 0x08020000UL;      // sector 5 — first uniform 128 KB sector
    cfg->size       = 128u * 1024u;      // one 128 KB sector
    cfg->write_unit = 4u;                // word (STM32F4 minimum write at VDD 2.7-3.6 V)
    cfg->erase_unit = 128u * 1024u;      // 128 KB sector (erase granularity)
    cfg->bad_base   = 0x07FFFFFFul;      // below STM32 flash start — not in sector table
}
