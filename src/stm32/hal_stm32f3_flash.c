// STM32F3 Flash HAL for LeafTS
// Reference: RM0316 (STM32F301/F302/F303/F318/F328/F334/F358/F373/F378/F398)
//
// Flash characteristics:
//   - Page size : 2048 bytes (all F3 devices are high-density equivalent)
//   - Write unit: 2 bytes (half-word)
//   - Erased state: 0xFF
//   - Flash base : 0x08000000
//
// Register layout identical to STM32F1 (KEYR/SR/CR/AR at same offsets,
// same FLASH_REG_BASE 0x40022000).  Only difference from F1: page size is
// always 2048 bytes regardless of device density.

#include <string.h>
#include <stdint.h>
#include "../../hal/stm32/hal_stm32f3_flash.h"

// ---------------------------------------------------------------------------
// FLASH register map (RM0316 §3.7 — identical to RM0008 STM32F1)
// ---------------------------------------------------------------------------
#define FLASH_REG_BASE  0x40022000UL
#define MCU_FLASH_BASE  0x08000000UL

#define FLASH_KEYR  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x04))
#define FLASH_SR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x0C))
#define FLASH_CR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x10))
#define FLASH_AR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x14))

#define SR_BSY      (1UL << 0)
#define SR_PGERR    (1UL << 2)
#define SR_WRPRTERR (1UL << 4)
#define SR_EOP      (1UL << 5)
#define SR_ALL_ERRORS (SR_PGERR | SR_WRPRTERR)

#define CR_PG   (1UL << 0)
#define CR_PER  (1UL << 1)
#define CR_STRT (1UL << 6)
#define CR_LOCK (1UL << 7)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

#define PAGE_SIZE_BYTES  2048u
#define HWORD_SIZE       2u

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint32_t g_base;
static uint32_t g_size;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
static void flash_unlock(void) {
    if (FLASH_CR & CR_LOCK) { FLASH_KEYR = FLASH_KEY1; FLASH_KEYR = FLASH_KEY2; }
}
static void flash_lock(void)      { FLASH_CR |= CR_LOCK; }
static void flash_wait_busy(void) { while (FLASH_SR & SR_BSY) {} }
static int  flash_check_errors(void) {
    if (FLASH_SR & SR_ALL_ERRORS) { FLASH_SR |= SR_ALL_ERRORS; return -1; }
    return 0;
}

// ---------------------------------------------------------------------------
// HAL interface
// ---------------------------------------------------------------------------
static int f3_read(uint32_t address, uint8_t *buffer, size_t size)
{
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    memcpy(buffer, (const void *)(uintptr_t)address, size);
    return 0;
}

static int f3_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    if (address % HWORD_SIZE != 0 || size % HWORD_SIZE != 0)             return -1;

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    FLASH_CR |= CR_PG;
    const uint16_t *src = (const uint16_t *)(uintptr_t)buffer;
    volatile uint16_t *dst = (volatile uint16_t *)(uintptr_t)address;

    for (size_t i = 0; i < size / HWORD_SIZE; i++) {
        dst[i] = src[i];
        flash_wait_busy();
        if (flash_check_errors() != 0) { FLASH_CR &= ~CR_PG; flash_lock(); return -1; }
    }

    FLASH_SR |= SR_EOP;
    FLASH_CR &= ~CR_PG;
    flash_lock();
    return 0;
}

static int f3_erase(uint32_t sector_address)
{
    if (sector_address < g_base ||
        sector_address + PAGE_SIZE_BYTES > g_base + g_size ||
        sector_address % PAGE_SIZE_BYTES != 0) return -1;

    flash_unlock();
    flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }

    FLASH_CR |= CR_PER;
    FLASH_AR  = sector_address;
    FLASH_CR |= CR_STRT;
    flash_wait_busy();

    int err = flash_check_errors();
    FLASH_CR &= ~CR_PER;
    flash_lock();
    if (err != 0 || !(FLASH_SR & SR_EOP)) return -1;
    FLASH_SR |= SR_EOP;
    return 0;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int stm32f3_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size)
{
    if (!flash || total_size == 0 || total_size % PAGE_SIZE_BYTES != 0) return -1;
    g_base = flash_base;
    g_size = total_size;
    flash->read        = f3_read;
    flash->write       = f3_write;
    flash->erase       = f3_erase;
    flash->total_size  = total_size;
    flash->sector_size = PAGE_SIZE_BYTES;
    flash->page_size   = HWORD_SIZE;
    return 0;
}
