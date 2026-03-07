// Mock MMIO test for STM32H7 flash HAL.
//
// Compiled with -DLEAFTS_MOCK_FLASH.  Runs on host (x86/x64).
//
// H7 has a unique register layout:
//   KEYR1 @ +0x04, CR1 @ +0x0C, SR1 @ +0x10, CCR1 @ +0x14.
// Write unit: 32 bytes (8 consecutive 32-bit words = one flash word).
// Erase unit: 128 KB sector.
#include "../unity/unity.h"
#include "../hal/stm32/hal_stm32h7_flash.h"
#include <string.h>
#include <stdint.h>

// 6 entries covers CCR1 at +0x14 (index 5).
static volatile uint32_t mock_regs[6];
#define MOCK_KEYR1  mock_regs[1]   // +0x04
#define MOCK_CR1    mock_regs[3]   // +0x0C
#define MOCK_SR1    mock_regs[4]   // +0x10
#define MOCK_CCR1   mock_regs[5]   // +0x14

// Sector 1 starts at 0x08020000 and is 128 KB.
#define TEST_LOG_BASE  0x08020000UL
#define TEST_SIZE      (128u * 1024u)
#define FLASH_WORD     32u

static uint8_t mock_flash_memory[TEST_SIZE];

void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base);

static hal_flash_t hal;

#define CR1_LOCK1   (1UL <<  0)
#define CR1_PG1     (1UL <<  1)
#define CR1_SER1    (1UL <<  2)
#define CR1_START1  (1UL <<  7)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

void setUp(void) {
    memset((void *)mock_regs, 0, sizeof(mock_regs));
    MOCK_CR1 = CR1_LOCK1;
    memset(mock_flash_memory, 0xFF, sizeof(mock_flash_memory));
    _mock_set_flash_bases((uintptr_t)mock_regs,
                          (uintptr_t)mock_flash_memory,
                          TEST_LOG_BASE);
    stm32h7_flash_init(&hal, TEST_LOG_BASE, TEST_SIZE);
}

void tearDown(void) {}

void test_unlock_writes_key2_last(void) {
    uint8_t data[FLASH_WORD];
    memset(data, 0xAA, FLASH_WORD);
    hal.write(TEST_LOG_BASE, data, FLASH_WORD);
    TEST_ASSERT_EQUAL_HEX32(FLASH_KEY2, MOCK_KEYR1);
}

void test_write_clears_pg1_after_completion(void) {
    uint8_t data[FLASH_WORD];
    memset(data, 0xAA, FLASH_WORD);
    hal.write(TEST_LOG_BASE, data, FLASH_WORD);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR1 & CR1_PG1);
}

void test_write_stores_data_in_mock_memory(void) {
    uint8_t data[FLASH_WORD];
    uint8_t readback[FLASH_WORD];
    memset(data, 0x5A, FLASH_WORD);
    memset(readback, 0, FLASH_WORD);
    TEST_ASSERT_EQUAL_INT(0, hal.write(TEST_LOG_BASE, data, FLASH_WORD));
    TEST_ASSERT_EQUAL_INT(0, hal.read (TEST_LOG_BASE, readback, FLASH_WORD));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, readback, FLASH_WORD);
}

void test_erase_returns_success(void) {
    TEST_ASSERT_EQUAL_INT(0, hal.erase(TEST_LOG_BASE));
}

void test_erase_clears_ser1_start1_after_completion(void) {
    hal.erase(TEST_LOG_BASE);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR1 & (CR1_SER1 | CR1_START1));
}

void test_read_returns_ff_on_fresh_erased_flash(void) {
    uint8_t buf[FLASH_WORD];
    memset(buf, 0, FLASH_WORD);
    hal.read(TEST_LOG_BASE, buf, FLASH_WORD);
    for (int i = 0; i < (int)FLASH_WORD; i++) TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unlock_writes_key2_last);
    RUN_TEST(test_write_clears_pg1_after_completion);
    RUN_TEST(test_write_stores_data_in_mock_memory);
    RUN_TEST(test_erase_returns_success);
    RUN_TEST(test_erase_clears_ser1_start1_after_completion);
    RUN_TEST(test_read_returns_ff_on_fresh_erased_flash);
    return UNITY_END();
}
