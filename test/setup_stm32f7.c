// STM32F7 configuration for the universal HAL flash contract tests.
// F7 uses 256 KB sectors (sectors 5-11 are uniform).

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32f7_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32f7_flash_init;
    cfg->base       = 0x08040000UL;      // sector 5 — first 256 KB sector
    cfg->size       = 256u * 1024u;      // one 256 KB sector
    cfg->write_unit = 4u;                // word (STM32F7 minimum write at VDD 2.7-3.6 V)
    cfg->erase_unit = 256u * 1024u;      // 256 KB sector (erase granularity)
    cfg->bad_base   = 0x07FFFFFFul;      // below STM32 flash start (0x08000000)
}
