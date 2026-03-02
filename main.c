#include <stdio.h>
#include "hal/hal_flash.h"
#include "hal/hal_vflash.h"

int main(void) {
    printf("LeafTS - Embedded Time-Series Database\n");

    // Initialize virtual flash HAL
    hal_flash_t hal;
    if (vflash_init(&hal) != 0) {
        printf("ERROR: Flash init failed!\n");
        return 1;
    }

    printf("Flash initialized: %u bytes total\n", hal.total_size);
    printf("Sector size: %u bytes\n", hal.sector_size);
    printf("Page size: %u bytes\n", hal.page_size);

    return 0;
}
