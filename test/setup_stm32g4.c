// STM32G4 configuration for the universal HAL flash contract tests.
// G4 uses 2 KB pages and double-word writes (same as STM32L4).

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32g4_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32g4_flash_init;
    cfg->base       = 0x08040000UL;  // first 256 KB reserved for firmware
    cfg->size       = 128u * 1024u;  // 128 KB LeafTS region (multiple of 2 KB page)
    cfg->write_unit = 8u;            // double-word (STM32G4 minimum write)
    cfg->erase_unit = 2048u;         // 2 KB page
    cfg->bad_base   = 0x07FFFFFFul;  // below STM32 flash start (0x08000000)
}
