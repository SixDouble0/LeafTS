#include <string.h>
#include "../include/leafts.h"

// CRC16-CCITT CALCULATION FUNCTION
static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)(data[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

// INITIALIZATION FUNCTION
int leafts_init(leafts_db_t *db, hal_flash_t *hal, uint32_t base_address, uint32_t size)
{
    if (db == NULL || hal == NULL) return LEAFTS_ERR_NULL; // NULL pointer error
    db->flash        = hal;           // Set flash HAL pointer
    db->base_address  = base_address; // Set base address for LeafTS data in flash
    db->region_size   = size;         // Store total size of reserved flash region
    db->capacity      = size / sizeof(leafts_record_t); // Calculate capacity based on flash size and record size
    db->record_count  = 0;            // Initialize record count to 0

    leafts_record_t record;
    for(uint32_t record_index = 0; record_index < db->capacity; record_index++){
        uint32_t flash_address = db->base_address + record_index * sizeof(leafts_record_t);
        if(db->flash->read(flash_address, (uint8_t*)&record, sizeof(leafts_record_t)) != 0){
            return LEAFTS_ERR_HAL; // HAL read error
        }
        if (record.magic == LEAFTS_MAGIC) {
            db->record_count++; // Increment record count for each valid record found
        } else {
            break; // Stop counting at the first invalid record (assuming records are stored sequentially)
        }
    }

    return LEAFTS_OK; // Success
}

// APPEND FUNCTION
int leafts_append(leafts_db_t *db, uint32_t timestamp, float value)
{
    if (db == NULL) return LEAFTS_ERR_NULL; // NULL pointer error
    if (db->record_count >= db->capacity) return LEAFTS_ERR_FULL; // Storage full error

    leafts_record_t record; // Create a new record
    record.magic     = LEAFTS_MAGIC; // Set magic number for integrity check
    record.timestamp = timestamp; // Set timestamp of the data point
    record.value     = value; // Set sensor reading or data value
    record.crc       = crc16((const uint8_t *)&record, sizeof(record) - sizeof(uint16_t)); // Calculate CRC16 checksum (excluding the crc field itself)

    // Calculate the flash address to write the new record (base address + offset based on current record count)
    uint32_t flash_address = db->base_address + db->record_count * sizeof(leafts_record_t);

    // Write the record to flash using the HAL write function
    if (db->flash->write(flash_address, (const uint8_t *)&record, sizeof(record)) != 0)
        return LEAFTS_ERR_HAL;

    db->record_count++; // Increment record count after successful write
    return LEAFTS_OK;
}

// GET LATEST FUNCTION
int leafts_get_latest(leafts_db_t *db, leafts_record_t *out)
{
    if (db == NULL || out == NULL) return LEAFTS_ERR_NULL; // NULL pointer error
    if (db->record_count == 0)    return LEAFTS_ERR_EMPTY; // Storage empty error

    // Calculate the flash address of the latest record (base address + offset based on current record count - 1)
    uint32_t flash_address = db->base_address + (db->record_count - 1) * sizeof(leafts_record_t);

    // Read the latest record from flash using the HAL read function
    if (db->flash->read(flash_address, (uint8_t *)out, sizeof(leafts_record_t)) != 0)
        return LEAFTS_ERR_HAL;// HAL read error

    // Verify the integrity of the retrieved record using the magic number and CRC16 checksum
    if (out->magic != LEAFTS_MAGIC)
        return LEAFTS_ERR_CRC; // Magic number check failed error

    // Recalculate CRC16 checksum and compare with the stored CRC value
    if (out->crc != crc16((const uint8_t *)out, sizeof(leafts_record_t) - sizeof(uint16_t)))
        return LEAFTS_ERR_CRC; // CRC check failed error

    return LEAFTS_OK; // Success
}

// GET RECORD BY INDEX FUNCTION
int leafts_get_by_index(leafts_db_t *db, uint32_t index, leafts_record_t *out)
{
    if (db == NULL || out == NULL) return LEAFTS_ERR_NULL;   // NULL pointer error
    if (index >= db->record_count) return LEAFTS_ERR_BOUNDS; // Index out of bounds error

    // Calculate flash address of the record at the given index
    uint32_t flash_address = db->base_address + index * sizeof(leafts_record_t);

    // Read the record from flash using the HAL read function
    if (db->flash->read(flash_address, (uint8_t *)out, sizeof(leafts_record_t)) != 0)
        return LEAFTS_ERR_HAL; // HAL read error

    // Verify magic number and CRC checksum for integrity
    if (out->magic != LEAFTS_MAGIC)
        return LEAFTS_ERR_CRC; // Magic number check failed error
    if (out->crc != crc16((const uint8_t *)out, sizeof(leafts_record_t) - sizeof(uint16_t)))
        return LEAFTS_ERR_CRC; // CRC check failed error

    return LEAFTS_OK; // Success
}

// ERASE DATABASE FUNCTION
int leafts_erase(leafts_db_t *db)
{
    if (db == NULL) return LEAFTS_ERR_NULL; // NULL pointer error

    // Calculate number of sectors to erase based on reserved region size
    uint32_t sector_count = db->region_size / db->flash->sector_size;

    // Erase each sector in the reserved region
    for (uint32_t sector_index = 0; sector_index < sector_count; sector_index++) {
        uint32_t sector_address = db->base_address + sector_index * db->flash->sector_size;
        if (db->flash->erase(sector_address) != 0)
            return LEAFTS_ERR_HAL; // HAL erase error
    }

    db->record_count = 0; // Reset record count after erase
    return LEAFTS_OK; // Success
}
