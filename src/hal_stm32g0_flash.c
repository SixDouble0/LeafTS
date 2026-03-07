// STM32G0 Flash HAL for LeafTS
// Reference: RM0444 (STM32G030/G031/G041/G070/G071/G081/G0B0/G0B1/G0C1)
//
// Flash characteristics:
//   - Page size  : 2048 bytes (erase unit) — same as STM32L4
//   - Write unit : 8 bytes (double-word, two consecutive 32-bit writes) — same as STM32L4
//   - Erased state: 0xFF
//   - Flash base : 0x08000000
//   - Single-bank only (no BKER bit in FLASH_CR, unlike STM32L4)
//
// Register layout is nearly identical to STM32L4 (same offsets, same bit positions).

#include <string.h>
#include <stdint.h>
#include "../hal/hal_stm32g0_flash.h"

// ---------------------------------------------------------------------------
// FLASH register map (RM0444 §3.6  — same base and offsets as STM32L4)
// ---------------------------------------------------------------------------
#ifndef FLASH_REG_BASE
#define FLASH_REG_BASE  0x40022000UL
#endif
#ifndef MCU_FLASH_BASE
#define MCU_FLASH_BASE  0x08000000UL
#endif

#define FLASH_KEYR  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x08))
#define FLASH_SR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x10))
#define FLASH_CR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x14))

// FLASH_SR bits (RM0444 §3.6.4) — identical to STM32L4
#define SR_EOP      (1UL <<  0)
#define SR_OPERR    (1UL <<  1)
#define SR_PROGERR  (1UL <<  3)
#define SR_WRPERR   (1UL <<  4)
#define SR_PGAERR   (1UL <<  5)
#define SR_SIZERR   (1UL <<  6)
#define SR_PGSERR   (1UL <<  7)
#define SR_BSY      (1UL << 16)

#define SR_ALL_ERRORS (SR_OPERR | SR_PROGERR | SR_WRPERR | SR_PGAERR | SR_SIZERR | SR_PGSERR)

// FLASH_CR bits (RM0444 §3.6.5)
#define CR_PG       (1UL <<  0)   // programming enable
#define CR_PER      (1UL <<  1)   // page erase enable
#define CR_PNB_POS  3             // page number field [9:3] — same position as STM32L4
#define CR_PNB_MASK (0x7FUL << CR_PNB_POS)  // 7-bit page number
#define CR_STRT     (1UL << 16)   // start erase
#define CR_LOCK     (1UL << 31)   // flash locked
// NOTE: no CR_BKER (bank select) on G0 — single-bank device only

// Unlock keys (same as all STM32 families)
#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

#define PAGE_SIZE_BYTES  2048u   // erase granularity
#define DWORD_SIZE       8u      // write granularity (double-word = 2 × 32-bit)

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint32_t g_base;
static uint32_t g_size;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
static void flash_unlock(void)
{
    if (FLASH_CR & CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
}

static void flash_lock(void)
{
    FLASH_CR |= CR_LOCK;
}

static void flash_wait_busy(void)
{
    while (FLASH_SR & SR_BSY) { /* spin */ }
}

static int flash_check_errors(void)
{
    if (FLASH_SR & SR_ALL_ERRORS) {
        FLASH_SR |= SR_ALL_ERRORS;  // clear by writing 1
        return -1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// HAL interface implementations
// ---------------------------------------------------------------------------

// read — flash is memory-mapped; direct memcpy.
static int g0_read(uint32_t address, uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }

    memcpy(buffer, (const void *)(uintptr_t)address, size);
    return 0;
}

// write — double-word (64-bit) programming via two consecutive 32-bit writes.
//         address and size must both be 8-byte aligned.
static int g0_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }
    if (address % DWORD_SIZE != 0)        { return -1; }
    if (size    % DWORD_SIZE != 0)        { return -1; }

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    FLASH_CR |= CR_PG;

    const uint32_t *src = (const uint32_t *)(uintptr_t)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)address;

    for (size_t i = 0; i < size / DWORD_SIZE; i++) {
        // Two 32-bit writes program one 64-bit double-word
        dst[0] = src[0];
        dst[1] = src[1];
        flash_wait_busy();
        if (flash_check_errors() != 0) {
            FLASH_CR &= ~CR_PG;
            flash_lock();
            return -1;
        }
        dst += 2;
        src += 2;
    }

    FLASH_SR |= SR_EOP;
    FLASH_CR &= ~CR_PG;
    flash_lock();
    return 0;
}

// erase — erases one 2 KB page.
//         sector_address must be 2 KB aligned and within the LeafTS region.
static int g0_erase(uint32_t sector_address)
{
    if (sector_address < g_base)                            { return -1; }
    if (sector_address + PAGE_SIZE_BYTES > g_base + g_size) { return -1; }
    if (sector_address % PAGE_SIZE_BYTES != 0)              { return -1; }

    // Calculate page number (0-based from start of physical flash)
    uint32_t page = (sector_address - (uint32_t)MCU_FLASH_BASE) / PAGE_SIZE_BYTES;

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    // Set PER + page number, then STRT
    FLASH_CR = (FLASH_CR & ~CR_PNB_MASK) | CR_PER | (page << CR_PNB_POS);
    FLASH_CR |= CR_STRT;
    flash_wait_busy();

    int err = flash_check_errors();
    FLASH_CR &= ~(CR_PER | CR_PNB_MASK);
    flash_lock();

    if (err != 0)             { return -1; }
    if (!(FLASH_SR & SR_EOP)) { return -1; }
    FLASH_SR |= SR_EOP;
    return 0;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int stm32g0_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size)
{
    if (!flash)                              { return -1; }
    if (total_size == 0)                     { return -1; }
    if (total_size % PAGE_SIZE_BYTES != 0)   { return -1; }

    g_base = flash_base;
    g_size = total_size;

    flash->read        = g0_read;
    flash->write       = g0_write;
    flash->erase       = g0_erase;
    flash->total_size  = total_size;
    flash->sector_size = PAGE_SIZE_BYTES;
    flash->page_size   = DWORD_SIZE;  // write granularity = 8 bytes
    return 0;
}
