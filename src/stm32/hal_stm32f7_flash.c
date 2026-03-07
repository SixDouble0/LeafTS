// STM32F7 Flash HAL for LeafTS
// Reference: RM0385 (STM32F74x / STM32F75x)
//
// Same flash controller as STM32F4 (FLASH_REG_BASE 0x40023C00),
// but the sector layout has 256 KB sectors instead of 128 KB (sectors 5-11).
// Write unit : 4 bytes (word, PSIZE=10 at VDD 2.7-3.6 V).
// Erased state: 0xFF.
//
// Sector map for 1 MB single-bank devices (RM0385 Table 4):
//   0 – 3 : 32 KB each
//   4      : 128 KB
//   5 – 11 : 256 KB each

#include <string.h>
#include <stdint.h>
#include "../../hal/stm32/hal_stm32f7_flash.h"

#ifndef FLASH_REG_BASE
#define FLASH_REG_BASE  0x40023C00UL
#endif
#ifndef MCU_FLASH_BASE
#define MCU_FLASH_BASE  0x08000000UL
#endif

// Register base indirection for host-side mock testing.
#ifdef LEAFTS_MOCK_FLASH
static uintptr_t s_reg_base = FLASH_REG_BASE;
static uintptr_t s_mem_base = 0;
static uint32_t  s_log_base = 0;
void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base) {
    s_reg_base = reg_base;
    s_mem_base = mem_base;
    s_log_base = log_base;
}
#define EFFECTIVE_REG_BASE  s_reg_base
#else
#define EFFECTIVE_REG_BASE  ((uintptr_t)FLASH_REG_BASE)
#endif

static inline void *flash_ptr(uint32_t address) {
#ifdef LEAFTS_MOCK_FLASH
    return (void *)(s_mem_base + (address - s_log_base));
#else
    return (void *)(uintptr_t)address;
#endif
}

#define FLASH_KEYR  (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x04))
#define FLASH_SR    (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x0C))
#define FLASH_CR    (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x10))

#define SR_EOP      (1UL <<  0)
#define SR_OPERR    (1UL <<  1)
#define SR_WRPERR   (1UL <<  4)
#define SR_PGAERR   (1UL <<  5)
#define SR_PGPERR   (1UL <<  6)
#define SR_PGSERR   (1UL <<  7)
#define SR_BSY      (1UL << 16)
#define SR_ALL_ERRORS (SR_OPERR | SR_WRPERR | SR_PGAERR | SR_PGPERR | SR_PGSERR)

#define CR_PG       (1UL <<  0)
#define CR_SER      (1UL <<  1)
#define CR_SNB_POS  3
#define CR_SNB_MASK (0xFUL << CR_SNB_POS)
#define CR_PSIZE_W  (2UL  <<  8)
#define CR_STRT     (1UL << 16)
#define CR_LOCK     (1UL << 31)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

#define SECTOR_256K_SIZE  (256u * 1024u)
#define WORD_SIZE         4u

typedef struct { uint32_t addr; uint32_t size; } f7_sector_t;

static const f7_sector_t k_sectors[] = {
    { 0x08000000UL,  32u * 1024u }, // sector  0
    { 0x08008000UL,  32u * 1024u }, // sector  1
    { 0x08010000UL,  32u * 1024u }, // sector  2
    { 0x08018000UL,  32u * 1024u }, // sector  3
    { 0x08020000UL, 128u * 1024u }, // sector  4
    { 0x08040000UL, 256u * 1024u }, // sector  5
    { 0x08080000UL, 256u * 1024u }, // sector  6
    { 0x080C0000UL, 256u * 1024u }, // sector  7
    { 0x08100000UL, 256u * 1024u }, // sector  8  (2 MB devices)
    { 0x08140000UL, 256u * 1024u }, // sector  9
    { 0x08180000UL, 256u * 1024u }, // sector 10
    { 0x081C0000UL, 256u * 1024u }, // sector 11
};
#define NUM_SECTORS  (sizeof(k_sectors) / sizeof(k_sectors[0]))

static uint32_t g_base;
static uint32_t g_size;

static void flash_unlock(void) {
    if (FLASH_CR & CR_LOCK) { FLASH_KEYR = FLASH_KEY1; FLASH_KEYR = FLASH_KEY2; }
}
static void flash_lock(void)      { FLASH_CR |= CR_LOCK; }
static void flash_wait_busy(void) { while (FLASH_SR & SR_BSY) {}
#ifdef LEAFTS_MOCK_FLASH
    FLASH_SR |= SR_EOP;
#endif
}
static int  flash_check_errors(void) {
    if (FLASH_SR & SR_ALL_ERRORS) { FLASH_SR |= SR_ALL_ERRORS; return -1; }
    return 0;
}
static int addr_to_sector(uint32_t address) {
    for (int i = 0; i < (int)NUM_SECTORS; i++) {
        if (address == k_sectors[i].addr) return i;
    }
    return -1;
}

static int f7_read(uint32_t address, uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    memcpy(buffer, flash_ptr(address), size);
    return 0;
}
static int f7_write(uint32_t address, const uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    if (address % WORD_SIZE != 0 || size % WORD_SIZE != 0) return -1;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    FLASH_CR = (FLASH_CR & ~CR_PSIZE_W) | CR_PG | CR_PSIZE_W;
    const uint32_t *src = (const uint32_t *)(uintptr_t)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)flash_ptr(address);
    for (size_t i = 0; i < size / WORD_SIZE; i++) {
        *dst++ = *src++;
        flash_wait_busy();
        if (flash_check_errors() != 0) { FLASH_CR &= ~CR_PG; flash_lock(); return -1; }
    }
    FLASH_SR |= SR_EOP; FLASH_CR &= ~CR_PG; flash_lock();
    return 0;
}
static int f7_erase(uint32_t sector_address) {
    int sn = addr_to_sector(sector_address);
    if (sn < 0 || sector_address < g_base ||
        sector_address + k_sectors[sn].size > g_base + g_size) return -1;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    FLASH_CR = (FLASH_CR & ~CR_SNB_MASK) | CR_SER | CR_PSIZE_W | ((uint32_t)sn << CR_SNB_POS);
    FLASH_CR |= CR_STRT; flash_wait_busy();
    int err = flash_check_errors();
    FLASH_CR &= ~(CR_SER | CR_SNB_MASK | CR_STRT); flash_lock();
    if (err != 0 || !(FLASH_SR & SR_EOP)) return -1;
    FLASH_SR |= SR_EOP;
    return 0;
}

int stm32f7_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size) {
    if (!flash) return -1;
    if (flash_base < MCU_FLASH_BASE) return -1;
    if (total_size == 0 || total_size % SECTOR_256K_SIZE != 0) return -1;
    g_base = flash_base; g_size = total_size;
    flash->read = f7_read; flash->write = f7_write; flash->erase = f7_erase;
    flash->total_size = total_size; flash->sector_size = SECTOR_256K_SIZE;
    flash->page_size = WORD_SIZE;
    return 0;
}
