// STM32L4 configuration for the universal HAL flash contract tests.
// This is the ONLY file that needs to be added for STM32L4 family coverage.
// For a new family: copy this file, change 4 numbers and the init function.

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32l4_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32l4_flash_init;
    cfg->base       = 0x08040000UL;  // first 256 KB reserved for firmware
    cfg->size       = 128u * 1024u;  // 128 KB LeafTS region (multiple of 2 KB page)
    cfg->write_unit = 8u;            // double-word (STM32L4 minimum write)
    cfg->erase_unit = 2048u;         // 2 KB page (STM32L4 erase unit)
    cfg->bad_base   = 0x07FFFFFFul;  // below STM32 flash start (0x08000000)
}
