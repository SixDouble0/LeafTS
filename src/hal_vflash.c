#include <string.h>
#include "../hal/hal_flash.h"

#define VFLASH_SIZE (64 * 1024) // 64KB virtual flash size(can be adjusted as needed)
#define SECTOR_SIZE (4 * 1024) // 4KB sector size
#define PAGE_SIZE (256) // 256B page size

static uint8_t vflash_memory[VFLASH_SIZE]; // Simulated flash memory

//READING FUNCTION
int vflash_read(uint32_t address, uint8_t* buffer, size_t size){
    if(buffer == NULL)                    { return -1; } // NULL pointer
    if(address + size > VFLASH_SIZE)      { return -1; } // Out of bounds
//         destination,   source,                  size
    memcpy(buffer,        &vflash_memory[address], size);
    return 0; // Success
}

//WRITING FUNCTION
int vflash_write(uint32_t address, const uint8_t* buffer, size_t size){
    if(buffer == NULL)                    { return -1; } // NULL pointer
    if(address + size > VFLASH_SIZE)      { return -1; } // Out of bounds

    for(size_t i = 0; i < size; i++) {
        // Simulate flash behavior: bits can only be changed from 1 to 0 without erasing
        vflash_memory[address + i] &= buffer[i];
    }
    return 0; // Success
}

//ERASING FUNCTION
int vflash_erase(uint32_t sector_address){
    if(sector_address % SECTOR_SIZE != 0) { return -1; } // Not aligned to sector boundary
    if(sector_address >= VFLASH_SIZE)     { return -1; } // Out of bounds
    //     destination,                   value, size
    memset(&vflash_memory[sector_address],0xFF,  SECTOR_SIZE); // Erase sets all bits to 1
    return 0; // Success
}

//INITIALIZATION FUNCTION
int vflash_init(hal_flash_t* flash){
    if(flash == NULL) { return -1; } // NULL pointer

    //     address,       clean bit,  size of data 
    memset(vflash_memory, 0xFF,       VFLASH_SIZE); // Initialize flash to all 1s (erased state)

    // Set function pointers 
    flash->read = vflash_read;
    flash->write = vflash_write;
    flash->erase = vflash_erase;
    // Set flash properties
    flash->total_size = VFLASH_SIZE;
    flash->sector_size = SECTOR_SIZE;
    flash->page_size = PAGE_SIZE;
    return 0; // Success
}