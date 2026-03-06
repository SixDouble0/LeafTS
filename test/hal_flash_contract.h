#ifndef HAL_FLASH_CONTRACT_H
#define HAL_FLASH_CONTRACT_H

#include "../hal/hal_flash.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// Parametric test configuration.
// Each MCU family provides ONE setup_<family>.c that fills this struct.
// The universal contract tests in hal_flash_contract.c use it transparently.
//
// Adding a new family = write a 15-line setup_XX.c, link it in CMake.
// No new test logic needed.
// ---------------------------------------------------------------------------
typedef struct {
    // Family init function
    int      (*init)(hal_flash_t *out, uint32_t base, uint32_t size);
    // A valid test region (base must be accepted by init)
    uint32_t   base;
    uint32_t   size;        // multiple of erase_unit
    uint32_t   write_unit;  // minimum write granularity (bytes, power of 2)
    uint32_t   erase_unit;  // erase page/sector size (bytes, power of 2)
    // An address that init() MUST reject (e.g. below MCU flash start)
    uint32_t   bad_base;
} HalFlashTestConfig;

// Implemented by setup_<family>.c
void hal_test_get_config(HalFlashTestConfig *cfg);

#endif // HAL_FLASH_CONTRACT_H
