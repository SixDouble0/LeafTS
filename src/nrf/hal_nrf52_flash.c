// nRF52 Flash HAL for LeafTS
// Reference: nRF52840 Product Specification v1.7, §4.3 NVMC (Non-Volatile Memory Controller)
//            (nRF52832 PS v1.4 is identical for NVMC)
//
// Flash characteristics:
//   - Page size   : 4096 bytes (erase unit)
//   - Write unit  : 4 bytes (32-bit word, 4-byte aligned)
//   - Erased state: 0xFF (all bits 1)
//   - Flash base  : 0x00000000 (code flash starts at address 0 on Cortex-M4)
//   - nRF52840    : 1 MB total (0x00000000 – 0x000FFFFF)
//   - nRF52832    : 512 KB total (0x00000000 – 0x0007FFFF)
//
// Write rules:
//   - CONFIG register must be set to Wen (0x01) before writing
//   - CONFIG must be set to Een (0x02) before erasing
//   - CONFIG is set back to ReadOnly (0x00) afterwards
//   - READY bit must be polled before each operation

#include <string.h>
#include <stdint.h>
#include "../../hal/nrf/hal_nrf52_flash.h"

// ---------------------------------------------------------------------------
// NVMC register map (nRF52840 PS §4.3.1)
// ---------------------------------------------------------------------------
#ifndef NVMC_BASE
#define NVMC_BASE  0x4001E000UL
#endif

// Register base indirection for host-side mock testing.
// NVMC registers are far apart (offsets 0x400, 0x504, 0x508), so the mock
// array must be large enough to cover 0x50C bytes = 323 uint32_t entries.
#ifdef LEAFTS_MOCK_FLASH
static uintptr_t s_reg_base = NVMC_BASE;
static uintptr_t s_mem_base = 0;
static uint32_t  s_log_base = 0;
void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base) {
    s_reg_base = reg_base;
    s_mem_base = mem_base;
    s_log_base = log_base;
}
#define EFFECTIVE_REG_BASE  s_reg_base
#else
#define EFFECTIVE_REG_BASE  ((uintptr_t)NVMC_BASE)
#endif

static inline void *flash_ptr(uint32_t address) {
#ifdef LEAFTS_MOCK_FLASH
    return (void *)(s_mem_base + (address - s_log_base));
#else
    return (void *)(uintptr_t)address;
#endif
}

#define NVMC_READY     (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x400))
#define NVMC_CONFIG    (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x504))
#define NVMC_ERASEPAGE (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x508))

#define NVMC_CONFIG_RO   0x00UL
#define NVMC_CONFIG_WEN  0x01UL
#define NVMC_CONFIG_EEN  0x02UL

#define PAGE_SIZE_BYTES  4096u    // erase granularity
#define WORD_SIZE        4u       // write granularity (32-bit word)

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint32_t g_base;
static uint32_t g_size;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
static void nvmc_wait_ready(void)
{
    while (!(NVMC_READY & 1UL)) { /* spin */ }
}

// ---------------------------------------------------------------------------
// HAL interface implementations
// ---------------------------------------------------------------------------

// read — flash is memory-mapped at its physical address; direct memcpy.
static int nrf_read(uint32_t address, uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }

    memcpy(buffer, flash_ptr(address), size);
    return 0;
}

// write — 32-bit word programming.
//         address and size must both be 4-byte aligned.
//         The target memory must be in the erased state (0xFFFFFFFF).
static int nrf_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }
    if (address % WORD_SIZE != 0)         { return -1; }
    if (size    % WORD_SIZE != 0)         { return -1; }

    nvmc_wait_ready();
    NVMC_CONFIG = NVMC_CONFIG_WEN;  // enable write

    const uint32_t *src = (const uint32_t *)(uintptr_t)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)flash_ptr(address);

    for (size_t i = 0; i < size / WORD_SIZE; i++) {
        dst[i] = src[i];
        nvmc_wait_ready();
    }

    NVMC_CONFIG = NVMC_CONFIG_RO;  // back to read-only
    return 0;
}

// erase — erases one 4 KB page.
//         sector_address must be 4 KB aligned and within the LeafTS region.
static int nrf_erase(uint32_t sector_address)
{
    if (sector_address < g_base)                               { return -1; }
    if (sector_address + PAGE_SIZE_BYTES > g_base + g_size)    { return -1; }
    if (sector_address % PAGE_SIZE_BYTES != 0)                 { return -1; }

    nvmc_wait_ready();
    NVMC_CONFIG    = NVMC_CONFIG_EEN;    // enable erase
    NVMC_ERASEPAGE = sector_address;     // writing the address triggers erase
    nvmc_wait_ready();
    NVMC_CONFIG    = NVMC_CONFIG_RO;     // back to read-only
    return 0;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int nrf52_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size)
{
    if (!flash)                              { return -1; }
    if (flash_base > 0x000FFFFFul)           { return -1; }   // above nRF52840 max (1 MB)
    if (total_size == 0)                     { return -1; }
    if (total_size % PAGE_SIZE_BYTES != 0)   { return -1; }

    g_base = flash_base;
    g_size = total_size;

    flash->read        = nrf_read;
    flash->write       = nrf_write;
    flash->erase       = nrf_erase;
    flash->total_size  = total_size;
    flash->sector_size = PAGE_SIZE_BYTES;
    flash->page_size   = WORD_SIZE;  // write granularity = 4 bytes
    return 0;
}
