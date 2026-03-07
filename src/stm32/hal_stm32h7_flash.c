// STM32H7 Flash HAL for LeafTS
// Reference: RM0433 (STM32H742/H743/H750/H753)
//
// The H7 flash controller differs significantly from all other STM32 families:
//   - FLASH_REG_BASE : 0x52002000 (bank 1, CPU1 access)
//   - Sector size    : 128 KB
//   - Flash word     : 32 bytes  (8 × uint32_t must be written consecutively)
//   - Write unit     : 32 bytes
//   - Erased state   : 0xFF
//   - CR bit layout  : completely different positions from L4 / F4

#include <string.h>
#include <stdint.h>
#include "../../hal/stm32/hal_stm32h7_flash.h"

// ---------------------------------------------------------------------------
// Register map (RM0433 §4.9, bank-1 base)
// ---------------------------------------------------------------------------
#ifndef FLASH_REG_BASE
#define FLASH_REG_BASE  0x52002000UL
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

#define FLASH_KEYR1  (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x04))
#define FLASH_CR1    (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x0C))
#define FLASH_SR1    (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x10))
#define FLASH_CCR1   (*(volatile uint32_t *)(EFFECTIVE_REG_BASE + 0x14))

// FLASH_CR1 bits (RM0433 §4.9.10)
#define CR1_LOCK1   (1UL <<  0)   // bank-1 lock
#define CR1_PG1     (1UL <<  1)   // programming enable (bank 1)
#define CR1_SER1    (1UL <<  2)   // sector erase
#define CR1_BER1    (1UL <<  3)   // bank erase
#define CR1_PSIZE1  (2UL <<  4)   // parallelism = 32-bit word (bits [5:4]=10)
#define CR1_START1  (1UL <<  7)   // start operation
#define CR1_SNB1_POS 8            // sector number [10:8]
#define CR1_SNB1_MASK (0x7UL << CR1_SNB1_POS)

// FLASH_SR1 bits (RM0433 §4.9.11)
#define SR1_BSY1    (1UL <<  0)   // busy
#define SR1_WBNE1   (1UL <<  1)   // write buffer not empty
#define SR1_QW1     (1UL <<  2)   // queue wait
#define SR1_EOP1    (1UL <<  4)   // end of program
#define SR1_WRPERR1 (1UL <<  5)
#define SR1_PGSERR1 (1UL <<  6)
#define SR1_STRBERR1 (1UL <<  7)
#define SR1_INCERR1 (1UL << 11)
#define SR1_ALL_ERRORS (SR1_WRPERR1|SR1_PGSERR1|SR1_STRBERR1|SR1_INCERR1)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

#define SECTOR_SIZE  (128u * 1024u)   // 128 KB
#define FLASH_WORD   32u              // 8 × 32-bit writes per flash word

static uint32_t g_base;
static uint32_t g_size;

static void flash_unlock(void) {
    if (FLASH_CR1 & CR1_LOCK1) { FLASH_KEYR1 = FLASH_KEY1; FLASH_KEYR1 = FLASH_KEY2; }
}
static void flash_lock(void)      { FLASH_CR1 |= CR1_LOCK1; }
static void flash_wait_busy(void) { while (FLASH_SR1 & (SR1_BSY1 | SR1_QW1 | SR1_WBNE1)) {} }
static int  flash_check_errors(void) {
    if (FLASH_SR1 & SR1_ALL_ERRORS) {
        FLASH_CCR1 |= SR1_ALL_ERRORS;   // clear flag register
        return -1;
    }
    return 0;
}

static int h7_read(uint32_t address, uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    memcpy(buffer, flash_ptr(address), size);
    return 0;
}

// Write in 32-byte flash-word units.
static int h7_write(uint32_t address, const uint8_t *buffer, size_t size) {
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    if (address % FLASH_WORD != 0 || size % FLASH_WORD != 0) return -1;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    FLASH_CR1 |= CR1_PG1;
    const uint32_t *src = (const uint32_t *)(uintptr_t)buffer;
    volatile uint32_t *dst = (volatile uint32_t *)flash_ptr(address);
    for (size_t words = 0; words < size / FLASH_WORD; words++) {
        // Write 8 consecutive 32-bit words (= 1 flash word = 32 bytes)
        for (int j = 0; j < 8; j++) dst[j] = src[j];
        flash_wait_busy();
        if (flash_check_errors() != 0) { FLASH_CR1 &= ~CR1_PG1; flash_lock(); return -1; }
        dst += 8; src += 8;
    }
    if (FLASH_SR1 & SR1_EOP1) FLASH_CCR1 |= SR1_EOP1;
    FLASH_CR1 &= ~CR1_PG1; flash_lock();
    return 0;
}

static int h7_erase(uint32_t sector_address) {
    if (sector_address < g_base ||
        sector_address + SECTOR_SIZE > g_base + g_size ||
        sector_address % SECTOR_SIZE != 0) return -1;
    uint32_t sn = (sector_address - (uint32_t)MCU_FLASH_BASE) / SECTOR_SIZE;
    flash_unlock(); flash_wait_busy();
    if (flash_check_errors() != 0) { flash_lock(); return -1; }
    FLASH_CR1 = (FLASH_CR1 & ~CR1_SNB1_MASK)
              | CR1_SER1 | CR1_PSIZE1 | (sn << CR1_SNB1_POS);
    FLASH_CR1 |= CR1_START1; flash_wait_busy();
    int err = flash_check_errors();
    // CR1_START1 is auto-cleared by hardware after the operation;
    // clear it explicitly here so the mock (which has no HW) is also clean.
    FLASH_CR1 &= ~(CR1_SER1 | CR1_SNB1_MASK | CR1_START1); flash_lock();
    if (err != 0) return -1;
    if (FLASH_SR1 & SR1_EOP1) FLASH_CCR1 |= SR1_EOP1;
    return 0;
}

int stm32h7_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size) {
    if (!flash) return -1;
    if (flash_base < (uint32_t)MCU_FLASH_BASE) return -1;
    if (total_size == 0 || total_size % SECTOR_SIZE != 0) return -1;
    g_base = flash_base; g_size = total_size;
    flash->read = h7_read; flash->write = h7_write; flash->erase = h7_erase;
    flash->total_size = total_size; flash->sector_size = SECTOR_SIZE;
    flash->page_size = FLASH_WORD;
    return 0;
}
