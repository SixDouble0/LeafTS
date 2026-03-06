// STM32L4 Flash HAL for LeafTS
// Reference: RM0394 (STM32L4x1/L4x2/L4x3/L4x5/L4x6 Reference Manual)
//
// Flash characteristics:
//   - Page size : 2048 bytes  (erase unit)
//   - Write unit: 8 bytes (double-word, two consecutive 32-bit writes)
//   - Erased state : 0xFF
//   - Flash base : 0x08000000

#include <string.h>
#include <stdint.h>
#include "../hal/hal_stm32l4_flash.h"

// ---------------------------------------------------------------------------
// FLASH register map (RM0394 §3.7)
// FLASH_REG_BASE and MCU_FLASH_BASE can be overridden at compile time
// (e.g. -DFLASH_REG_BASE=... -DMCU_FLASH_BASE=...) for host-side mock testing.
// ---------------------------------------------------------------------------
#ifndef FLASH_REG_BASE
#define FLASH_REG_BASE   0x40022000UL
#endif

#define FLASH_KEYR      (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x08))
#define FLASH_SR        (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x10))
#define FLASH_CR        (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x14))

// FLASH_SR bits
#define SR_EOP      (1UL <<  0)   // end of operation
#define SR_OPERR    (1UL <<  1)   // operation error
#define SR_PROGERR  (1UL <<  3)   // programming error
#define SR_WRPERR   (1UL <<  4)   // write-protection error
#define SR_PGAERR   (1UL <<  5)   // alignment error
#define SR_SIZERR   (1UL <<  6)   // size error
#define SR_PGSERR   (1UL <<  7)   // sequence error
#define SR_BSY      (1UL << 16)   // flash busy

#define SR_ALL_ERRORS (SR_OPERR | SR_PROGERR | SR_WRPERR | SR_PGAERR | SR_SIZERR | SR_PGSERR)

// FLASH_CR bits
#define CR_PG       (1UL <<  0)   // programming enable
#define CR_PER      (1UL <<  1)   // page erase enable
#define CR_PNB_POS  3             // page number field starts at bit 3
#define CR_PNB_MASK (0x7FUL << CR_PNB_POS)  // 7-bit page number
#define CR_BKER     (1UL << 11)   // bank select (dual-bank devices: 0=bank1, 1=bank2)
#define CR_STRT     (1UL << 16)   // start erase
#define CR_LOCK     (1UL << 31)   // flash locked

// Unlock keys
#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

// Hardware constants
#define PAGE_SIZE_BYTES  2048u    // bytes per page (erase granularity)
#define DWORD_SIZE       8u       // double-word write granularity (bytes)
#ifndef MCU_FLASH_BASE
#define MCU_FLASH_BASE   0x08000000UL  // STM32L4 flash always starts here
#endif

// ---------------------------------------------------------------------------
// Register base indirection - overridable at runtime for host mock testing
// In production these never change.  In tests compiled with -DLEAFTS_MOCK_FLASH
// the test calls _mock_set_flash_bases() to point to a RAM struct instead.
// ---------------------------------------------------------------------------
#ifdef LEAFTS_MOCK_FLASH
static uintptr_t s_reg_base = FLASH_REG_BASE;
static uintptr_t s_mcu_base = MCU_FLASH_BASE;
static uintptr_t s_mem_base = 0;       // actual RAM backing the "flash" in tests
static uint32_t  s_log_base = 0;       // logical uint32 address that maps to s_mem_base
void _mock_set_flash_bases(uintptr_t reg_base,
                           uintptr_t mem_base, uint32_t log_base) {
    s_reg_base = reg_base;
    s_mcu_base = (uintptr_t)log_base;  // page-number calc uses this as uint32 anyway
    s_mem_base = mem_base;
    s_log_base = log_base;
}
#define EFFECTIVE_REG_BASE  s_reg_base
#define EFFECTIVE_MCU_BASE  s_mcu_base
#else
#define EFFECTIVE_REG_BASE  ((uintptr_t)FLASH_REG_BASE)
#define EFFECTIVE_MCU_BASE  ((uintptr_t)MCU_FLASH_BASE)
#endif

// ---------------------------------------------------------------------------
// State kept by this driver
// ---------------------------------------------------------------------------
static uint32_t g_base;   // first address LeafTS uses
static uint32_t g_size;   // how many bytes LeafTS may use

// Translates a logical flash address to an actual memory pointer.
// Production (32-bit ARM): identity mapping - address IS the pointer.
// Mock (x86-64): maps to the RAM buffer supplied by the test.
static inline void *flash_ptr(uint32_t address) {
#ifdef LEAFTS_MOCK_FLASH
    return (void *)(s_mem_base + (address - s_log_base));
#else
    return (void *)(uintptr_t)address;
#endif
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

static void flash_unlock(void) {
    if (FLASH_CR & CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
}

static void flash_lock(void) {
    FLASH_CR |= CR_LOCK;
}

static void flash_wait_busy(void) {
    while (FLASH_SR & SR_BSY) { /* spin */ }
}

static int flash_check_errors(void) {
    if (FLASH_SR & SR_ALL_ERRORS) {
        FLASH_SR |= SR_ALL_ERRORS;   // clear by writing 1
        return -1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// HAL interface implementations
// ---------------------------------------------------------------------------

// read  - flash is memory-mapped, so just memcpy from the address directly.
static int l4_read(uint32_t address, uint8_t *buffer, size_t size) {
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }

    memcpy(buffer, flash_ptr(address), size);
    return 0;
}

// write  - STM32L4 requires 8-byte (double-word) aligned programming.
//          The caller is expected to align writes to DWORD_SIZE.
//          Unaligned writes are rejected to match hardware behaviour.
static int l4_write(uint32_t address, const uint8_t *buffer, size_t size) {
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }
    if (address % DWORD_SIZE != 0)        { return -1; }   // must be 8-byte aligned
    if (size   % DWORD_SIZE != 0)         { return -1; }   // must be multiple of 8 bytes

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    FLASH_CR |= CR_PG;  // enable programming

    const uint32_t *src = (const uint32_t *)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)flash_ptr(address);

    for (size_t i = 0; i < size / DWORD_SIZE; i++) {
        // Write two 32-bit words back-to-back to program one double-word
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

    // Clear EOP flag
    FLASH_SR |= SR_EOP;
    FLASH_CR &= ~CR_PG;
    flash_lock();
    return 0;
}

// erase  - erases one 2 KB page.
//          sector_address must be 2 KB aligned and within the LeafTS region.
static int l4_erase(uint32_t sector_address) {
    if (sector_address < g_base)                       { return -1; }
    if (sector_address + PAGE_SIZE_BYTES > g_base + g_size) { return -1; }
    if (sector_address % PAGE_SIZE_BYTES != 0)         { return -1; }

    // Convert address to page number (0-based from start of physical flash)
    uint32_t offset = sector_address - (uint32_t)EFFECTIVE_MCU_BASE;
    uint32_t page   = offset / PAGE_SIZE_BYTES;

    // Dual-bank devices: pages 0-255 = bank1, pages 256-511 = bank2
    uint32_t bank = 0;
    if (page >= 256u) {
        bank  = 1;
        page -= 256u;
    }

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    // Set page erase: PER | PNB | BKER
    uint32_t cr = FLASH_CR;
    cr &= ~(CR_PNB_MASK | CR_BKER);             // clear page number and bank select
    cr |= CR_PER;                                // page erase enable
    cr |= (page << CR_PNB_POS) & CR_PNB_MASK;   // set page number
    if (bank) cr |= CR_BKER;                     // select bank 2 if needed
    FLASH_CR = cr;

    FLASH_CR |= CR_STRT;    // start erase
    flash_wait_busy();

    int result = flash_check_errors();
    FLASH_SR |= SR_EOP;
    FLASH_CR &= ~(CR_PER | CR_PNB_MASK | CR_BKER | CR_STRT);  // clear all erase bits
    flash_lock();
    return result;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

int stm32l4_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size) {
    if (!flash)                              { return -1; }
    if (flash_base < (uint32_t)EFFECTIVE_MCU_BASE)         { return -1; }
    if (total_size == 0)                     { return -1; }
    if (total_size % PAGE_SIZE_BYTES != 0)   { return -1; }  // must be page-aligned

    g_base = flash_base;
    g_size = total_size;

    flash->read        = l4_read;
    flash->write       = l4_write;
    flash->erase       = l4_erase;
    flash->total_size  = total_size;
    flash->sector_size = PAGE_SIZE_BYTES;   // erase granularity
    flash->page_size   = DWORD_SIZE;        // write granularity (double-word)

    return 0;
}
