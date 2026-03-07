// STM32F1 Flash HAL for LeafTS
// Reference: RM0008 (STM32F101xx/F102xx/F103xx/F105xx/F107xx Reference Manual)
//
// Flash characteristics:
//   - Page size : 1024 bytes (medium-density, ≤128 KB total flash)
//                 2048 bytes (high-density / XL-density, >128 KB total flash)
//   - Write unit: 2 bytes (half-word)
//   - Erased state: 0xFF
//   - Flash base : 0x08000000

#include <string.h>
#include <stdint.h>
#include "../hal/hal_stm32f1_flash.h"

// ---------------------------------------------------------------------------
// FLASH register map (RM0008 §3.3)
// ---------------------------------------------------------------------------
#ifndef FLASH_REG_BASE
#define FLASH_REG_BASE  0x40022000UL
#endif
#ifndef MCU_FLASH_BASE
#define MCU_FLASH_BASE  0x08000000UL
#endif

#define FLASH_KEYR  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x04))
#define FLASH_SR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x0C))
#define FLASH_CR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x10))
#define FLASH_AR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x14))

// FLASH_SR bits
#define SR_BSY      (1UL << 0)   // flash busy
#define SR_PGERR    (1UL << 2)   // programming error (target not erased)
#define SR_WRPRTERR (1UL << 4)   // write protection error
#define SR_EOP      (1UL << 5)   // end of operation

#define SR_ALL_ERRORS (SR_PGERR | SR_WRPRTERR)

// FLASH_CR bits
#define CR_PG   (1UL << 0)   // programming enable
#define CR_PER  (1UL << 1)   // page erase enable
#define CR_STRT (1UL << 6)   // start erase
#define CR_LOCK (1UL << 7)   // flash locked

// Unlock keys (same sequence as STM32L4 / STM32F4)
#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

// Write granularity: half-word (16-bit)
#define HWORD_SIZE  2u

// ---------------------------------------------------------------------------
// State kept by this driver
// ---------------------------------------------------------------------------
static uint32_t g_base;
static uint32_t g_size;
static uint32_t g_page_bytes;  // 1024 or 2048 depending on device density

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
        FLASH_SR |= SR_ALL_ERRORS;   // clear by writing 1
        return -1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// HAL interface implementations
// ---------------------------------------------------------------------------

// read — flash is memory-mapped; direct memcpy.
static int f1_read(uint32_t address, uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }

    memcpy(buffer, (const void *)(uintptr_t)address, size);
    return 0;
}

// write — STM32F1 requires half-word (16-bit) programming.
//         address and size must both be 2-byte aligned.
//         The target location must be in the erased state (0xFFFF) beforehand.
static int f1_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }
    if (address % HWORD_SIZE != 0)        { return -1; }
    if (size    % HWORD_SIZE != 0)        { return -1; }

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    FLASH_CR |= CR_PG;  // enable half-word programming

    const uint16_t *src = (const uint16_t *)(uintptr_t)buffer;
    volatile uint16_t *dst = (volatile uint16_t *)(uintptr_t)address;

    for (size_t i = 0; i < size / HWORD_SIZE; i++) {
        dst[i] = src[i];
        flash_wait_busy();
        if (flash_check_errors() != 0) {
            FLASH_CR &= ~CR_PG;
            flash_lock();
            return -1;
        }
    }

    FLASH_SR |= SR_EOP;
    FLASH_CR &= ~CR_PG;
    flash_lock();
    return 0;
}

// erase — erases one page.
//         sector_address must be page-aligned and within the LeafTS region.
static int f1_erase(uint32_t sector_address)
{
    if (sector_address < g_base)                               { return -1; }
    if (sector_address + g_page_bytes > g_base + g_size)       { return -1; }
    if (sector_address % g_page_bytes != 0)                    { return -1; }

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    FLASH_CR |= CR_PER;         // page erase mode
    FLASH_AR  = sector_address; // set target page address
    FLASH_CR |= CR_STRT;        // start erase
    flash_wait_busy();

    int err = flash_check_errors();
    FLASH_CR &= ~CR_PER;
    flash_lock();

    if (err != 0)             { return -1; }
    if (!(FLASH_SR & SR_EOP)) { return -1; }
    FLASH_SR |= SR_EOP;
    return 0;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int stm32f1_flash_init(hal_flash_t *flash,
                       uint32_t flash_base, uint32_t total_size,
                       uint32_t page_bytes)
{
    if (!flash)                                         { return -1; }
    if (page_bytes != 1024u && page_bytes != 2048u)     { return -1; }
    if (total_size == 0)                                { return -1; }
    if (total_size % page_bytes != 0)                   { return -1; }

    g_base       = flash_base;
    g_size       = total_size;
    g_page_bytes = page_bytes;

    flash->read        = f1_read;
    flash->write       = f1_write;
    flash->erase       = f1_erase;
    flash->total_size  = total_size;
    flash->sector_size = page_bytes;
    flash->page_size   = HWORD_SIZE;  // write granularity = 2 bytes
    return 0;
}
