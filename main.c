#include <stdio.h>
#include "hal/hal_flash.h"
#include "hal/hal_vflash.h"
#include "include/leafts.h"

#define DB_BASE_ADDR  0x00000000U
#define DB_SIZE       (4U * 1024U)   // 4KB reserved for LeafTS records

int main(void) {
    printf("LeafTS - Embedded Time-Series Database\n\n");

    // INITIALIZE VIRTUAL FLASH HAL
    hal_flash_t hal;
    if (vflash_init(&hal) != 0) {
        printf("ERROR: Flash init failed!\n");
        return 1;
    }
    printf("Flash: %u bytes, sector=%u, page=%u\n\n",
           hal.total_size, hal.sector_size, hal.page_size);

    // INITIALIZE LEAFTS DATABASE
    leafts_db_t db;
    if (leafts_init(&db, &hal, DB_BASE_ADDR, DB_SIZE) != LEAFTS_OK) {
        printf("ERROR: leafts_init failed!\n");
        return 1;
    }
    printf("DB: capacity=%u records, count=%u\n\n", db.capacity, db.record_count);

    // APPEND 3 RECORDS TO FLASH
    printf("Writing records...\n");
    leafts_append(&db, 1000, 21.75f);
    leafts_append(&db, 1060, 22.10f);
    leafts_append(&db, 1120, 22.50f);
    printf("Records written: %u\n\n", db.record_count);

    // READ LATEST RECORD FROM FLASH
    leafts_record_t latest_record;
    if (leafts_get_latest(&db, &latest_record) != LEAFTS_OK) {
        printf("ERROR: get_latest failed!\n");
        return 1;
    }
    printf("Latest record:\n");
    printf("  timestamp = %u\n", latest_record.timestamp);
    printf("  value     = %.2f\n", latest_record.value);
    printf("  crc       = 0x%04X\n\n", latest_record.crc);

    // SIMULATE POWER-CYCLE (RE-INIT FROM FLASH)
    printf("Simulating power-cycle...\n");
    leafts_db_t db_restored;
    leafts_init(&db_restored, &hal, DB_BASE_ADDR, DB_SIZE);
    printf("After power-cycle: count=%u (expected: 3)\n", db_restored.record_count);

    return 0;
}
