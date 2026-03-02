#ifndef HAL_VFLASH_H
#define HAL_VFLASH_H

#include "hal_flash.h"

// Initialize virtual flash HAL (PC simulation only)
int vflash_init(hal_flash_t* flash);

#endif // HAL_VFLASH_H
