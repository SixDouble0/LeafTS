// STM32F1 configuration for the universal HAL flash contract tests.
// STM32F1 init takes an extra page_bytes parameter, so a thin wrapper is used
// to match the standard (hal_flash_t*, uint32_t, uint32_t) signature.

#include "hal_flash_contract.h"
#include "../hal/stm32/hal_stm32f1_flash.h"

// Wrapper: Always use 2048-byte pages (high-density / XL-density devices).
static int stm32f1_flash_init_2k(hal_flash_t *out, uint32_t base, uint32_t size) {
    return stm32f1_flash_init(out, base, size, 2048u);
}

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = stm32f1_flash_init_2k;
    cfg->base       = 0x08010000UL;  // first 64 KB reserved for firmware
    cfg->size       = 64u * 1024u;   // 64 KB LeafTS region (multiple of 2 KB page)
    cfg->write_unit = 2u;            // half-word (STM32F1 minimum write)
    cfg->erase_unit = 2048u;         // 2 KB page (high-density erase unit)
    cfg->bad_base   = 0x07FFFFFFul;  // below STM32 flash start (0x08000000)
}
