// STM32G4 Flash HAL for LeafTS
// Reference: RM0440 (STM32G431/G441/G471/G473/G474/G483/G484/G491/G4A1)
//
// Flash controller identical to STM32L4 (same FLASH_REG_BASE 0x40022000,
// same register offsets, same CR bit positions).
// Single-bank implementation — no BKER (suitable for G431/G441 128 KB devices).
// For dual-bank G474/G484: set BKER in CR_PNB area if page >= 128.

#include <string.h>
#include <stdint.h>
#include "../../hal/stm32/hal_stm32g4_flash.h"

#define FLASH_REG_BASE  0x40022000UL
#define MCU_FLASH_BASE  0x08000000UL

#define FLASH_KEYR  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x08))
#define FLASH_SR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x10))
#define FLASH_CR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x14))

#define SR_EOP      (1UL <<  0)
#define SR_OPERR    (1UL <<  1)
#define SR_PROGERR  (1UL <<  3)
#define SR_WRPERR   (1UL <<  4)
#define SR_PGAERR   (1UL <<  5)
#define SR_SIZERR   (1UL <<  6)
#define SR_PGSERR   (1UL <<  7)
#define SR_BSY      (1UL << 16)
#define SR_ALL_ERRORS (SR_OPERR|SR_PROGERR|SR_WRPERR|SR_PGAERR|SR_SIZERR|SR_PGSERR)

#define CR_PG       (1UL <<  0)
#define CR_PER      (1UL <<  1)
#define CR_PNB_POS  3
#define CR_PNB_MASK (0x7FUL << CR_PNB_POS)
#define CR_STRT     (1UL << 16)
#define CR_LOCK     (1UL << 31)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL
#define PAGE_SIZE_BYTES  2048u
#define DWORD_SIZE       8u

static uint32_t g_base;
static uint32_t g_size;

static void flash_unlock(void) {
    if (FLASH_CR & CR_LOCK) { FLASH_KEYR = FLASH_KEY1; FLASH_KEYR = FLASH_KEY2; }
}
static void flash_lock(void)      { FLASH_CR |= CR_LOCK; }
static void flash_wait_busy(void) { while (FLASH_SR & SR_BSY) {} }
static int  flash_check_errors(void) {
    if (FLASH_SR & SR_ALL_ERRORS) { FLASH_SR |= SR_ALL_ERRORS; return -1; }
    return 0;
}

static int g4_read(uint32_t address, uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    memcpy(buffer, (const void *)(uintptr_t)address, size);
    return 0;
}

static int g4_write(uint32_t address, const uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    if (address % DWORD_SIZE != 0 || size % DWORD_SIZE != 0)             return -1;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    FLASH_CR |= CR_PG;
    const uint32_t *src = (const uint32_t *)(uintptr_t)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)address;
    for (size_t i = 0; i < size / DWORD_SIZE; i++) {
        dst[0] = src[0]; dst[1] = src[1];
        flash_wait_busy();
        if (flash_check_errors() != 0) { FLASH_CR &= ~CR_PG; flash_lock(); return -1; }
        dst += 2; src += 2;
    }
    FLASH_SR |= SR_EOP; FLASH_CR &= ~CR_PG; flash_lock();
    return 0;
}

static int g4_erase(uint32_t sector_address) {
    if (sector_address < g_base ||
        sector_address + PAGE_SIZE_BYTES > g_base + g_size ||
        sector_address % PAGE_SIZE_BYTES != 0) return -1;
    uint32_t page = (sector_address - MCU_FLASH_BASE) / PAGE_SIZE_BYTES;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    FLASH_CR = (FLASH_CR & ~CR_PNB_MASK) | CR_PER | (page << CR_PNB_POS);
    FLASH_CR |= CR_STRT; flash_wait_busy();
    int err = flash_check_errors();
    FLASH_CR &= ~(CR_PER | CR_PNB_MASK); flash_lock();
    if (err != 0 || !(FLASH_SR & SR_EOP)) return -1;
    FLASH_SR |= SR_EOP;
    return 0;
}

int stm32g4_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size) {
    if (!flash || total_size == 0 || total_size % PAGE_SIZE_BYTES != 0) return -1;
    g_base = flash_base; g_size = total_size;
    flash->read = g4_read; flash->write = g4_write; flash->erase = g4_erase;
    flash->total_size = total_size; flash->sector_size = PAGE_SIZE_BYTES;
    flash->page_size = DWORD_SIZE;
    return 0;
}
