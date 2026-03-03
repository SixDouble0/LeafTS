#include <stdio.h>
#include <string.h>
#include "hal/hal_flash.h"
#include "hal/hal_vflash.h"
#include "include/leafts.h"

#define DB_BASE_ADDR  0x00000000U
#define DB_SIZE       (4U * 1024U)   // 4KB reserved for LeafTS records

// PRINT SINGLE RECORD HELPER
static void print_record(uint32_t index, const leafts_record_t *record)
{
    printf("  [%u] timestamp=%-10u  value=%.2f  crc=0x%04X\n",
           index, record->timestamp, record->value, record->crc);
}

// PRINT AVAILABLE COMMANDS
static void print_help(void)
{
    printf("Commands:\n");
    printf("  append <timestamp> <value>  - Write new record to flash\n");
    printf("  latest                      - Read latest record\n");
    printf("  list                        - List all records\n");
    printf("  erase                       - Erase entire database\n");
    printf("  status                      - Show DB info\n");
    printf("  exit                        - Quit\n\n");
}

int main(void)
{
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
    printf("DB ready: capacity=%u records, count=%u\n\n", db.capacity, db.record_count);

    // START CLI LOOP
    print_help();
    char command[64];
    while (1) {
        printf("> ");
        if (fgets(command, sizeof(command), stdin) == NULL) break;

        // PARSE AND HANDLE APPEND COMMAND
        uint32_t timestamp;
        float value;
        if (sscanf(command, "append %u %f", &timestamp, &value) == 2) {
            int result = leafts_append(&db, timestamp, value);
            if (result == LEAFTS_OK)
                printf("[OK] Record appended (count=%u)\n\n", db.record_count);
            else if (result == LEAFTS_ERR_FULL)
                printf("[ERROR] Database is full (capacity=%u)\n\n", db.capacity);
            else
                printf("[ERROR] HAL write failed\n\n");

        // PARSE AND HANDLE LATEST COMMAND
        } else if (strncmp(command, "latest", 6) == 0) {
            leafts_record_t latest_record;
            int result = leafts_get_latest(&db, &latest_record);
            if (result == LEAFTS_OK)
                print_record(db.record_count - 1, &latest_record);
            else if (result == LEAFTS_ERR_EMPTY)
                printf("[EMPTY] No records in database\n");
            else
                printf("[ERROR] CRC or HAL error\n");
            printf("\n");

        // PARSE AND HANDLE LIST COMMAND
        } else if (strncmp(command, "list", 4) == 0) {
            if (db.record_count == 0) {
                printf("[EMPTY] No records in database\n\n");
            } else {
                leafts_record_t current_record;
                for (uint32_t record_index = 0; record_index < db.record_count; record_index++) {
                    if (leafts_get_by_index(&db, record_index, &current_record) == LEAFTS_OK)
                        print_record(record_index, &current_record);
                    else
                        printf("  [%u] ERROR reading record\n", record_index);
                }
                printf("\n");
            }

        // PARSE AND HANDLE ERASE COMMAND
        } else if (strncmp(command, "erase", 5) == 0) {
            if (leafts_erase(&db) == LEAFTS_OK)
                printf("[OK] Database erased (count=%u)\n\n", db.record_count);
            else
                printf("[ERROR] HAL erase failed\n\n");

        // PARSE AND HANDLE STATUS COMMAND
        } else if (strncmp(command, "status", 6) == 0) {
            printf("  base_address = 0x%08X\n", db.base_address);
            printf("  region_size  = %u bytes\n", db.region_size);
            printf("  capacity     = %u records\n", db.capacity);
            printf("  record_count = %u\n\n", db.record_count);

        // PARSE AND HANDLE EXIT COMMAND
        } else if (strncmp(command, "exit", 4) == 0) {
            printf("Bye!\n");
            break;

        // HANDLE UNKNOWN COMMAND
        } else if (command[0] != '\n') {
            printf("[?] Unknown command. Type 'help' for list.\n\n");
        }
    }

    return 0;
}
