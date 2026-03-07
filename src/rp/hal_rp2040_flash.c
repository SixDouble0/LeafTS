// RP2040 Flash HAL for LeafTS
// Reference: RP2040 Datasheet §4.7 (Flash), §2.8 (Bootrom)
//
// The RP2040 stores code and data in external QSPI flash.
// The flash is accessible via XIP mapping at 0x10000000, but writes/erases
// must go through the ROM boot-ROM functions (executed from RAM/ROM).
//
// Erase granularity : 4096 bytes (standard SFDP sector erase)
// Write granularity : 256 bytes (page program)
// Erased state      : 0xFF
//
// On real hardware (PICO_RP2040 defined by the Pico SDK build system),
// this uses the pico-sdk flash helpers.  When building for the host
// (compile-check), stubs are compiled in so the object file is valid.

#include <string.h>
#include <stdint.h>
#include "../../hal/rp/hal_rp2040_flash.h"

#define XIP_BASE         0x10000000UL
#define SECTOR_SIZE      4096u
#define PAGE_PROG_SIZE   256u

static uint32_t g_base;
static uint32_t g_size;

#ifdef PICO_RP2040
// ---------------------------------------------------------------------------
// Real RP2040 implementation — requires pico-sdk
// ---------------------------------------------------------------------------
#include "hardware/flash.h"
#include "hardware/sync.h"

static int rp_read(uint32_t address, uint8_t *buffer, size_t size)
{
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    memcpy(buffer, (const void *)(uintptr_t)address, size);
    return 0;
}

static int rp_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    if (address % PAGE_PROG_SIZE != 0 || size % PAGE_PROG_SIZE != 0) return -1;
    uint32_t offset = address - XIP_BASE;
    uint32_t irq = save_and_disable_interrupts();
    flash_range_program(offset, buffer, size);
    restore_interrupts(irq);
    return 0;
}

static int rp_erase(uint32_t sector_address)
{
    if (sector_address < g_base ||
        sector_address + SECTOR_SIZE > g_base + g_size ||
        sector_address % SECTOR_SIZE != 0) return -1;
    uint32_t offset = sector_address - XIP_BASE;
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(offset, SECTOR_SIZE);
    restore_interrupts(irq);
    return 0;
}

#else
// ---------------------------------------------------------------------------
// Host stub — compile-check only; not for actual MCU use
// ---------------------------------------------------------------------------
static int rp_read(uint32_t address, uint8_t *buffer, size_t size)
{
    (void)address; (void)buffer; (void)size;
    return -1;
}
static int rp_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    (void)address; (void)buffer; (void)size;
    return -1;
}
static int rp_erase(uint32_t sector_address)
{
    (void)sector_address;
    return -1;
}
#endif  // PICO_RP2040

int rp2040_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size)
{
    if (!flash) return -1;
    if (flash_base < XIP_BASE) return -1;   // must be in XIP window
    if (total_size == 0 || total_size % SECTOR_SIZE != 0) return -1;
    g_base = flash_base; g_size = total_size;
    flash->read  = rp_read;
    flash->write = rp_write;
    flash->erase = rp_erase;
    flash->total_size  = total_size;
    flash->sector_size = SECTOR_SIZE;
    flash->page_size   = PAGE_PROG_SIZE;
    return 0;
}
