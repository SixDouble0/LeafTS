// STM32L1 configuration for the universal HAL flash contract tests.
// L1 uses 256-byte pages and word writes via PECR mechanism.

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32l1_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32l1_flash_init;
    cfg->base       = 0x08010000UL;  // first 64 KB reserved for firmware
    cfg->size       = 64u * 1024u;   // 64 KB LeafTS region (multiple of 256-byte page)
    cfg->write_unit = 4u;            // word (STM32L1 minimum write)
    cfg->erase_unit = 256u;          // 256-byte page (erase granularity)
    cfg->bad_base   = 0x07FFFFFFul;  // below STM32 flash start (0x08000000)
}
