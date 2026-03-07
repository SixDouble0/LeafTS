// nRF52 configuration for the universal HAL flash contract tests.
// nRF52 uses 4 KB pages and word writes via NVMC controller.
// Flash starts at 0x00000000, upper limit: 1 MB (nRF52840).

#include "hal_flash_contract.h"
#include "../hal/nrf/hal_nrf52_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = nrf52_flash_init;
    cfg->base       = 0x00080000UL;  // 512 KB in — past typical firmware image
    cfg->size       = 4u * 4096u;    // 16 KB LeafTS region (four 4 KB pages)
    cfg->write_unit = 4u;            // word (nRF52 minimum write)
    cfg->erase_unit = 4096u;         // 4 KB page
    cfg->bad_base   = 0x00100000ul;  // at/above 1 MB — rejected by init guard
}
