#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "include/leafts.h"
#include "include/platform_hal.h"

#define DB_BASE_ADDR  0x00000000U
#define DB_SIZE       (4U * 1024U)   // 4KB reserved for LeafTS records

static uint32_t auto_timestamp_now(const leafts_db_t *db)
{
    time_t now = time(NULL);
    if (now > 0 && (unsigned long long)now <= 0xFFFFFFFFULL)
        return (uint32_t)now;

    if (db->record_count > 0) {
        leafts_record_t latest_record;
        if (leafts_get_latest((leafts_db_t *)db, &latest_record) == LEAFTS_OK)
            return latest_record.timestamp + 1U;
    }

    return 1U;
}

// PRINT SINGLE RECORD HELPER
static void print_record(uint32_t index, const leafts_record_t *record)
{
    printf("  [%u] timestamp=%-10u  value=%.2f  crc=0x%04X\n",
           index, record->timestamp, record->value, record->crc);
}

static int erase_n_smallest_records(leafts_db_t *db, uint32_t n, uint32_t *removed_out)
{
    if (!db || !removed_out) return LEAFTS_ERR_NULL;
    if (db->record_count == 0) return LEAFTS_ERR_EMPTY;
    if (n == 0) return LEAFTS_ERR_NULL;

    uint32_t total = db->record_count;
    if (n >= total) {
        int r = leafts_erase(db);
        if (r == LEAFTS_OK) *removed_out = total;
        return r;
    }

    leafts_record_t *records = (leafts_record_t *)malloc((size_t)total * sizeof(leafts_record_t));
    uint8_t *remove_mask = (uint8_t *)calloc(total, sizeof(uint8_t));
    if (!records || !remove_mask) {
        free(records);
        free(remove_mask);
        return LEAFTS_ERR_NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        if (leafts_get_by_index(db, i, &records[i]) != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return LEAFTS_ERR_HAL;
        }
    }

    for (uint32_t k = 0; k < n; k++) {
        uint32_t min_idx = total;
        float min_val = 0.0f;
        for (uint32_t i = 0; i < total; i++) {
            if (remove_mask[i]) continue;
            if (min_idx == total || records[i].value < min_val) {
                min_idx = i;
                min_val = records[i].value;
            }
        }
        if (min_idx == total) break;
        remove_mask[min_idx] = 1U;
    }

    int r = leafts_erase(db);
    if (r != LEAFTS_OK) {
        free(records);
        free(remove_mask);
        return r;
    }

    uint32_t removed = 0;
    for (uint32_t i = 0; i < total; i++) {
        if (remove_mask[i]) {
            removed++;
            continue;
        }
        r = leafts_append(db, records[i].timestamp, records[i].value);
        if (r != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return r;
        }
    }

    free(records);
    free(remove_mask);
    *removed_out = removed;
    return LEAFTS_OK;
}

static int erase_n_largest_records(leafts_db_t *db, uint32_t n, uint32_t *removed_out)
{
    if (!db || !removed_out) return LEAFTS_ERR_NULL;
    if (db->record_count == 0) return LEAFTS_ERR_EMPTY;
    if (n == 0) return LEAFTS_ERR_NULL;

    uint32_t total = db->record_count;
    if (n >= total) {
        int r = leafts_erase(db);
        if (r == LEAFTS_OK) *removed_out = total;
        return r;
    }

    leafts_record_t *records = (leafts_record_t *)malloc((size_t)total * sizeof(leafts_record_t));
    uint8_t *remove_mask = (uint8_t *)calloc(total, sizeof(uint8_t));
    if (!records || !remove_mask) {
        free(records);
        free(remove_mask);
        return LEAFTS_ERR_NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        if (leafts_get_by_index(db, i, &records[i]) != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return LEAFTS_ERR_HAL;
        }
    }

    for (uint32_t k = 0; k < n; k++) {
        uint32_t max_idx = total;
        float max_val = 0.0f;
        for (uint32_t i = 0; i < total; i++) {
            if (remove_mask[i]) continue;
            if (max_idx == total || records[i].value > max_val) {
                max_idx = i;
                max_val = records[i].value;
            }
        }
        if (max_idx == total) break;
        remove_mask[max_idx] = 1U;
    }

    int r = leafts_erase(db);
    if (r != LEAFTS_OK) {
        free(records);
        free(remove_mask);
        return r;
    }

    uint32_t removed = 0;
    for (uint32_t i = 0; i < total; i++) {
        if (remove_mask[i]) {
            removed++;
            continue;
        }
        r = leafts_append(db, records[i].timestamp, records[i].value);
        if (r != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return r;
        }
    }

    free(records);
    free(remove_mask);
    *removed_out = removed;
    return LEAFTS_OK;
}

// PRINT AVAILABLE COMMANDS
static void print_help(void)
{
    printf("Commands:\n");
    printf("  insert <value>              - Add record (auto timestamp)\n");
    printf("  insert <value> <timestamp>  - Add record (manual timestamp)\n");
    printf("  select                      - Latest record\n");
    printf("  select *                    - List all records\n");
    printf("  select count(*)             - Record count\n");
    printf("  select * where timestamp between <from> <to> - Range query\n");
    printf("  delete from leafts          - Remove all records\n");
    printf("  delete min(value)           - Remove smallest record\n");
    printf("  delete max(value)           - Remove largest record\n");
    printf("  delete from leafts order by value asc limit <n> - Remove N smallest values\n");
    printf("  delete from leafts order by value desc limit <n> - Remove N largest values\n");
    printf("  erase min <n>               - Remove N smallest values\n");
    printf("  erase max <n>               - Remove N largest values\n");
    printf("  erase                       - Remove all records\n");
    printf("  clear                       - Clear terminal output\n");
    printf("  status                      - Database info\n");
    printf("  help                        - Show this list\n");
    printf("  exit                        - Quit\n\n");
}

int main(void)
{
    printf("LeafTS - Embedded Time-Series Database\n\n");

    // Initialize selected platform flash HAL (virtual by default).
    hal_flash_t hal;
    if (platform_flash_init(&hal, DB_BASE_ADDR, DB_SIZE) != 0) {
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
        char extra;
        int has_manual_ts =
            (sscanf(command, "append %f %u %c", &value, &timestamp, &extra) == 2) ||
            (sscanf(command, "insert %f %u %c", &value, &timestamp, &extra) == 2);
        int has_auto_ts =
            (sscanf(command, "append %f %c", &value, &extra) == 1) ||
            (sscanf(command, "insert %f %c", &value, &extra) == 1);

        if (has_manual_ts || has_auto_ts) {
            if (!has_manual_ts) {
                timestamp = auto_timestamp_now(&db);
            }
            int result = leafts_append(&db, timestamp, value);
            if (result == LEAFTS_OK)
                printf("[OK] Record appended (count=%u)\n\n", db.record_count);
            else if (result == LEAFTS_ERR_FULL)
                printf("[ERROR] Database is full (capacity=%u)\n\n", db.capacity);
            else
                printf("[ERROR] HAL write failed\n\n");

        // PARSE AND HANDLE SELECT * / LIST COMMAND
        } else if (strncmp(command, "select *", 8) == 0 ||
                   strncmp(command, "select all", 10) == 0 ||
                   strncmp(command, "list", 4) == 0) {
            uint32_t ts_from = 0;
            uint32_t ts_to = 0;
            if (sscanf(command, "select * where timestamp between %u %u", &ts_from, &ts_to) == 2) {
                uint32_t shown = 0;
                leafts_record_t current_record;
                for (uint32_t record_index = 0; record_index < db.record_count; record_index++) {
                    if (leafts_get_by_index(&db, record_index, &current_record) == LEAFTS_OK) {
                        if (current_record.timestamp >= ts_from && current_record.timestamp <= ts_to) {
                            print_record(record_index, &current_record);
                            shown++;
                        }
                    }
                }
                if (shown == 0)
                    printf("[EMPTY] No records in given range\n");
                printf("\n");
                continue;
            }

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

        // PARSE AND HANDLE LATEST COMMAND
        } else if (strncmp(command, "latest", 6) == 0 ||
                   strncmp(command, "select latest", 13) == 0 ||
                   strcmp(command, "select\n") == 0 ||
                   strcmp(command, "select") == 0) {
            leafts_record_t latest_record;
            int result = leafts_get_latest(&db, &latest_record);
            if (result == LEAFTS_OK)
                print_record(db.record_count - 1, &latest_record);
            else if (result == LEAFTS_ERR_EMPTY)
                printf("[EMPTY] No records in database\n");
            else
                printf("[ERROR] CRC or HAL error\n");
            printf("\n");

        // PARSE AND HANDLE HELP COMMAND
        } else if (strncmp(command, "help", 4) == 0) {
            print_help();

        // PARSE AND HANDLE SQL COUNT
        } else if (strncmp(command, "select count(*)", 15) == 0) {
            printf("  count = %u\n\n", db.record_count);

        // PARSE AND HANDLE DELETE N SMALLEST COMMAND
        } else if (sscanf(command, "erase min %u", &timestamp) == 1 ||
                   sscanf(command, "delete from leafts order by value asc limit %u", &timestamp) == 1) {
            uint32_t removed = 0;
            int result = erase_n_smallest_records(&db, timestamp, &removed);
            if (result == LEAFTS_OK)
                printf("[OK] Removed %u smallest records (count=%u)\n\n", removed, db.record_count);
            else if (result == LEAFTS_ERR_EMPTY)
                printf("[EMPTY] No records in database\n\n");
            else
                printf("[ERROR] Delete failed (code=%d)\n\n", result);

        // PARSE AND HANDLE DELETE LARGEST COMMAND
        } else if (sscanf(command, "erase max %u", &timestamp) == 1 ||
                   sscanf(command, "delete from leafts order by value desc limit %u", &timestamp) == 1 ||
                   strcmp(command, "delete max(value)\n") == 0 ||
                   strcmp(command, "delete max(value)") == 0) {
            uint32_t n = timestamp;
            if (strcmp(command, "delete max(value)\n") == 0 || strcmp(command, "delete max(value)") == 0)
                n = 1;
            uint32_t removed = 0;
            int result = erase_n_largest_records(&db, n, &removed);
            if (result == LEAFTS_OK)
                printf("[OK] Removed %u largest records (count=%u)\n\n", removed, db.record_count);
            else if (result == LEAFTS_ERR_EMPTY)
                printf("[EMPTY] No records in database\n\n");
            else
                printf("[ERROR] Delete failed (code=%d)\n\n", result);

        // PARSE AND HANDLE ERASE COMMAND
        } else if (strncmp(command, "erase", 5) == 0 ||
                   strncmp(command, "delete from leafts", 18) == 0 ||
                   strncmp(command, "truncate table leafts", 21) == 0) {
            if (leafts_erase(&db) == LEAFTS_OK)
                printf("[OK] Database erased (count=%u)\n\n", db.record_count);
            else
                printf("[ERROR] HAL erase failed\n\n");

        // PARSE AND HANDLE CLEAR COMMAND
        } else if (strncmp(command, "clear", 5) == 0) {
            printf("\033[2J\033[H");

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
