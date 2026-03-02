#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    // POINTER FOR READING FUNCTION

    //          address: The starting address in flash memory to read from.
    //                           buffer: A pointer to a buffer where the read data will be stored.
    //                                            size: The number of bytes to read.  
    int (*read)(uint32_t address, uint8_t* buffer, size_t size);
    
    // POINTER FOR WRITING FUNCTION

    //          address: The starting address in flash memory to write to.
    //                           buffer: A pointer to the data to be written.
    //                                            size: The number of bytes to write.
    int (*write)(uint32_t address, const uint8_t* buffer, size_t size);
    
    // POINTER FOR ERASING FUNCTION

    //          sector_address: The starting address of the sector to be erased.
    int (*erase)(uint32_t sector_address);
    
    // FLASH MEMORY PROPERTIES
    uint32_t total_size;    // How many bytes in total does the flash memory have?
    uint32_t sector_size;   // How many bytes in a sector? (for erase)
    uint32_t page_size;     // How many bytes in a page? (for write)
    
} hal_flash_t;



#endif // HAL_FLASH_H