// ESP32 configuration for the universal HAL flash contract tests.
// ESP32 uses 4 KB erase sectors and 256-byte page-program units.
// flash_base is a byte offset within the SPI flash (0-based, up to 4 MB).

#include "hal_flash_contract.h"
#include "../hal/esp32/hal_esp32_flash.h"

void hal_test_get_config(HalFlashTestConfig *cfg) {
    cfg->init       = esp32_flash_init;
    cfg->base       = 0x00040000UL;  // 256 KB offset — past OTA/partition table
    cfg->size       = 4u * 4096u;    // 16 KB LeafTS region (four 4 KB sectors)
    cfg->write_unit = 256u;          // page-program granularity
    cfg->erase_unit = 4096u;         // 4 KB sector erase
    cfg->bad_base   = 0x00400000ul;  // at 4 MB boundary: combined with any size → exceeds limit
}
