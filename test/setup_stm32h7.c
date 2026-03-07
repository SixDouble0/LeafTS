// STM32H7 configuration for the universal HAL flash contract tests.
// H7 uses 128 KB sectors and 32-byte flash-word writes.

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32h7_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32h7_flash_init;
    cfg->base       = 0x08020000UL;      // sector 1 (after sector 0 firmware)
    cfg->size       = 128u * 1024u;      // one 128 KB sector
    cfg->write_unit = 32u;               // flash word = 8 × 32-bit writes
    cfg->erase_unit = 128u * 1024u;      // 128 KB sector
    cfg->bad_base   = 0x07FFFFFFul;      // below STM32 flash start (0x08000000)
}
