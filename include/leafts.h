#ifndef LEAFTS_H
#define LEAFTS_H

#include <stdint.h>
#include <stddef.h>
#include "../hal/hal_flash.h"

// Magic number to identify valid LeafTS flash storage (for integrity checks)
#define LEAFTS_MAGIC 0x4C54U  // "LT" in ASCII :)

// ERROR CODES
#define LEAFTS_OK 0
#define LEAFTS_ERR_NULL (-1) // NULL pointer error
#define LEAFTS_ERR_FULL (-2) // Storage full error
#define LEAFTS_ERR_EMPTY (-3) // Storage empty error
#define LEAFTS_ERR_CRC (-4) // Magic or CRC check failed error
#define LEAFTS_ERR_HAL (-5) // HAL operation error


// DATA STRUCTURE FOR A SINGLE TIME-SERIES RECORD
// Packed to ensure no padding bytes, which is crucial for flash storage
typedef struct __attribute__((packed)) {
    uint16_t magic;      // Magic number for integrity check
    uint32_t timestamp;  // Timestamp of the data point (e.g., Unix time)
    float value;         // Sensor reading or data value
    uint16_t crc;        // CRC16 checksum for integrity verification
} leafts_record_t; // 12 bytes total (2 (magic) + 4(timestamp) + 4(value) + 2(crc))

// DATA BASE STRUCTURE 
typedef struct {
    hal_flash_t* flash;  // Pointer to the flash HAL interface
    uint32_t base_address; // first address in flash where LeafTS data is stored
    uint32_t capacity;    // Maximum number of records that can be stored (calculated from flash size)
    uint32_t record_count; // Current number of records stored
}leafts_db_t; 

// API DECLARATIONS
int leafts_init(leafts_db_t *db, hal_flash_t *hal, uint32_t base_addr, uint32_t size);

int leafts_append(leafts_db_t *db, uint32_t timestamp, float value);

int leafts_get_latest(leafts_db_t *db, leafts_record_t *out);

#endif // LEAFTS_H