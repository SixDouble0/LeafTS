// STM32L1 Flash HAL for LeafTS
// Reference: RM0038 (STM32L151/L152/L162)
//
// The L1 flash controller uses the PECR (Program/Erase Control Register)
// mechanism, which is completely different from all other STM32 families.
// Key characteristics:
//   - FLASH_REG_BASE : 0x40023C00
//   - Page size      : 256 bytes
//   - Write unit     : 4 bytes (word-aligned)
//   - Erased state   : 0x00000000 (not 0xFF!)
//   - No KEYR (uses PEKEYR + PRGKEYR two-key sequence instead)
//   - Erase: set PROG+ERASE in PECR, write to any word in target page

#include <string.h>
#include <stdint.h>
#include "../../hal/stm32/hal_stm32l1_flash.h"

#define FLASH_REG_BASE   0x40023C00UL
#define MCU_FLASH_BASE   0x08000000UL

#define FLASH_PECR    (*(volatile uint32_t *)(FLASH_REG_BASE + 0x04))
#define FLASH_PEKEYR  (*(volatile uint32_t *)(FLASH_REG_BASE + 0x0C))
#define FLASH_PRGKEYR (*(volatile uint32_t *)(FLASH_REG_BASE + 0x10))
#define FLASH_SR      (*(volatile uint32_t *)(FLASH_REG_BASE + 0x18))

// PECR bits (RM0038 §3.7.2)
#define PECR_PELOCK   (1UL <<  0)   // program/erase lock
#define PECR_PRGLOCK  (1UL <<  1)   // program memory lock
#define PECR_PROG     (1UL <<  3)   // select program memory
#define PECR_ERASE    (1UL <<  9)   // erase mode

// SR bits (RM0038 §3.7.6)
#define SR_BSY        (1UL <<  0)
#define SR_READY      (1UL <<  3)
#define SR_WRPERR     (1UL << 16)
#define SR_PGAERR     (1UL << 17)
#define SR_SIZERR     (1UL << 18)
#define SR_ALL_ERRORS (SR_WRPERR | SR_PGAERR | SR_SIZERR)

// Two-stage unlock keys (RM0038 §3.7.3–3.7.4)
#define PEKEY1   0x89ABCDEFUL
#define PEKEY2   0x02030405UL
#define PRGKEY1  0x8C9DAEBFUL
#define PRGKEY2  0x13141516UL

#define PAGE_SIZE_BYTES  256u
#define WORD_SIZE        4u

static uint32_t g_base;
static uint32_t g_size;

// Unlock program memory: write two keys to PEKEYR (clears PELOCK),
// then two keys to PRGKEYR (clears PRGLOCK).
static void flash_unlock(void) {
    if (FLASH_PECR & PECR_PELOCK) {
        FLASH_PEKEYR = PEKEY1;
        FLASH_PEKEYR = PEKEY2;
    }
    if (FLASH_PECR & PECR_PRGLOCK) {
        FLASH_PRGKEYR = PRGKEY1;
        FLASH_PRGKEYR = PRGKEY2;
    }
}

static void flash_lock(void) {
    FLASH_PECR |= PECR_PELOCK;   // re-locks both PRGLOCK and PELOCK
}

static void flash_wait_busy(void) {
    while (FLASH_SR & SR_BSY) {}
}

static int flash_check_errors(void) {
    if (FLASH_SR & SR_ALL_ERRORS) {
        FLASH_SR |= SR_ALL_ERRORS;
        return -1;
    }
    return 0;
}

static int l1_read(uint32_t address, uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    memcpy(buffer, (const void *)(uintptr_t)address, size);
    return 0;
}

static int l1_write(uint32_t address, const uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    if (address % WORD_SIZE != 0 || size % WORD_SIZE != 0) return -1;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    // Set PROG to enable word programming
    FLASH_PECR |= PECR_PROG;
    const uint32_t *src = (const uint32_t *)(uintptr_t)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)address;
    for (size_t i = 0; i < size / WORD_SIZE; i++) {
        *dst++ = *src++;
        flash_wait_busy();
        if (flash_check_errors() != 0) {
            FLASH_PECR &= ~PECR_PROG; flash_lock(); return -1;
        }
    }
    FLASH_PECR &= ~PECR_PROG; flash_lock();
    return 0;
}

static int l1_erase(uint32_t page_address) {
    if (page_address < g_base ||
        page_address + PAGE_SIZE_BYTES > g_base + g_size ||
        page_address % PAGE_SIZE_BYTES != 0) return -1;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    // Set PROG + ERASE in PECR; writing to any word in the page triggers erase
    FLASH_PECR |= PECR_PROG | PECR_ERASE;
    *(volatile uint32_t *)(uintptr_t)page_address = 0x00000000UL;
    flash_wait_busy();
    int err = flash_check_errors();
    FLASH_PECR &= ~(PECR_PROG | PECR_ERASE); flash_lock();
    return err;
}

int stm32l1_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size) {
    if (!flash || total_size == 0 || total_size % PAGE_SIZE_BYTES != 0) return -1;
    g_base = flash_base; g_size = total_size;
    flash->read = l1_read; flash->write = l1_write; flash->erase = l1_erase;
    flash->total_size = total_size; flash->sector_size = PAGE_SIZE_BYTES;
    flash->page_size = WORD_SIZE;
    return 0;
}
