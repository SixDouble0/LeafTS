// STM32F4 Flash HAL for LeafTS
// Reference: RM0090 (STM32F405/F407/F415/F417/F427/F429/F437/F439)
//
// Flash characteristics:
//   - Non-uniform sector sizes (see header for full map)
//   - LeafTS uses only the uniform 128 KB sectors (sectors 5-11)
//   - Write unit : 4 bytes (word, at VDD 2.7-3.6 V with PSIZE=10)
//   - Erased state: 0xFF
//   - Flash base : 0x08000000

#include <string.h>
#include <stdint.h>
#include "../../hal/stm32/hal_stm32f4_flash.h"

// ---------------------------------------------------------------------------
// FLASH register map (RM0090 §3.8)
// ---------------------------------------------------------------------------
#define FLASH_REG_BASE  0x40023C00UL
#define MCU_FLASH_BASE  0x08000000UL

#define FLASH_KEYR  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x04))
#define FLASH_SR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x0C))
#define FLASH_CR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x10))

// FLASH_SR bits
#define SR_EOP      (1UL <<  0)   // end of operation
#define SR_OPERR    (1UL <<  1)   // operation error
#define SR_WRPERR   (1UL <<  4)   // write protection error
#define SR_PGAERR   (1UL <<  5)   // alignment error
#define SR_PGPERR   (1UL <<  6)   // parallelism error
#define SR_PGSERR   (1UL <<  7)   // sequence error
#define SR_BSY      (1UL << 16)   // flash busy

#define SR_ALL_ERRORS (SR_OPERR | SR_WRPERR | SR_PGAERR | SR_PGPERR | SR_PGSERR)

// FLASH_CR bits
#define CR_PG       (1UL <<  0)   // programming enable
#define CR_SER      (1UL <<  1)   // sector erase enable
#define CR_SNB_POS  3             // sector number field starts at bit 3  (bits [6:3])
#define CR_SNB_MASK (0xFUL << CR_SNB_POS)
#define CR_PSIZE_W  (2UL  <<  8)  // parallelism = word (32-bit), valid at VDD 2.7-3.6 V
#define CR_STRT     (1UL << 16)   // start erase
#define CR_LOCK     (1UL << 31)   // flash locked

// Unlock keys (same as STM32L4 / STM32F1)
#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

#define SECTOR_128K_SIZE  (128u * 1024u)   // erase granularity for sectors 5-11
#define WORD_SIZE         4u               // write granularity (bytes)

// ---------------------------------------------------------------------------
// Sector map for STM32F4 single-bank 1 MB devices (RM0090 Table 5)
// addr = sector start address, size = sector size in bytes
// ---------------------------------------------------------------------------
typedef struct { uint32_t addr; uint32_t size; } f4_sector_t;

static const f4_sector_t k_sectors[] = {
    { 0x08000000UL,  16u * 1024u }, // sector  0
    { 0x08004000UL,  16u * 1024u }, // sector  1
    { 0x08008000UL,  16u * 1024u }, // sector  2
    { 0x0800C000UL,  16u * 1024u }, // sector  3
    { 0x08010000UL,  64u * 1024u }, // sector  4
    { 0x08020000UL, 128u * 1024u }, // sector  5
    { 0x08040000UL, 128u * 1024u }, // sector  6
    { 0x08060000UL, 128u * 1024u }, // sector  7
    { 0x08080000UL, 128u * 1024u }, // sector  8
    { 0x080A0000UL, 128u * 1024u }, // sector  9
    { 0x080C0000UL, 128u * 1024u }, // sector 10
    { 0x080E0000UL, 128u * 1024u }, // sector 11
};
#define NUM_SECTORS  (sizeof(k_sectors) / sizeof(k_sectors[0]))

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
        FLASH_SR |= SR_ALL_ERRORS;   // clear by writing 1
        return -1;
    }
    return 0;
}

// Returns the sector index for a given start address, or -1 if not found.
static int addr_to_sector(uint32_t address)
{
    for (int i = 0; i < (int)NUM_SECTORS; i++) {
        if (address == k_sectors[i].addr) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// HAL interface implementations
// ---------------------------------------------------------------------------

// read — flash is memory-mapped; direct memcpy.
static int f4_read(uint32_t address, uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }

    memcpy(buffer, (const void *)(uintptr_t)address, size);
    return 0;
}

// write — 32-bit word programming.
//         address and size must both be 4-byte aligned.
static int f4_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    if (!buffer)                          { return -1; }
    if (address < g_base)                 { return -1; }
    if (address + size > g_base + g_size) { return -1; }
    if (address % WORD_SIZE != 0)         { return -1; }
    if (size    % WORD_SIZE != 0)         { return -1; }

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    // Enable word programming with PSIZE=10 (VDD 2.7-3.6 V)
    FLASH_CR = (FLASH_CR & ~(CR_SNB_MASK)) | CR_PG | CR_PSIZE_W;

    const uint32_t *src = (const uint32_t *)(uintptr_t)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)address;

    for (size_t i = 0; i < size / WORD_SIZE; i++) {
        dst[i] = src[i];
        flash_wait_busy();
        if (flash_check_errors() != 0) {
            FLASH_CR &= ~(CR_PG | CR_PSIZE_W);
            flash_lock();
            return -1;
        }
    }

    FLASH_SR |= SR_EOP;
    FLASH_CR &= ~(CR_PG | CR_PSIZE_W);
    flash_lock();
    return 0;
}

// erase — erases one 128 KB sector.
//         sector_address must be the start of a valid 128 KB sector (5-11).
static int f4_erase(uint32_t sector_address)
{
    if (sector_address < g_base)                               { return -1; }
    if (sector_address + SECTOR_128K_SIZE > g_base + g_size)   { return -1; }

    int sn = addr_to_sector(sector_address);
    if (sn < 0) { return -1; }

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    FLASH_CR = CR_SER | CR_PSIZE_W | ((uint32_t)sn << CR_SNB_POS);
    FLASH_CR |= CR_STRT;
    flash_wait_busy();

    int err = flash_check_errors();
    FLASH_CR &= ~(CR_SER | CR_SNB_MASK | CR_PSIZE_W);
    flash_lock();

    if (err != 0)             { return -1; }
    if (!(FLASH_SR & SR_EOP)) { return -1; }
    FLASH_SR |= SR_EOP;
    return 0;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int stm32f4_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size)
{
    if (!flash)                                { return -1; }
    if (total_size == 0)                       { return -1; }
    if (total_size % SECTOR_128K_SIZE != 0)    { return -1; }
    if (addr_to_sector(flash_base) < 0)        { return -1; }  // must start at a sector boundary

    g_base = flash_base;
    g_size = total_size;

    flash->read        = f4_read;
    flash->write       = f4_write;
    flash->erase       = f4_erase;
    flash->total_size  = total_size;
    flash->sector_size = SECTOR_128K_SIZE;
    flash->page_size   = WORD_SIZE;  // write granularity = 4 bytes
    return 0;
}
