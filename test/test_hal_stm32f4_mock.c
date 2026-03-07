// Mock MMIO test for STM32F4 flash HAL.
//
// Compiled with -DLEAFTS_MOCK_FLASH.  Runs on host (x86/x64).
//
// F4-style register layout: KEYR@+0x04, SR@+0x0C, CR@+0x10.
// Sector 5 of STM32F4 starts at 0x08020000 and is 128 KB.
#include "../unity/unity.h"
#include "../hal/stm32/hal_stm32f4_flash.h"
#include <string.h>
#include <stdint.h>

static volatile uint32_t mock_regs[5];
#define MOCK_KEYR  mock_regs[1]   // +0x04
#define MOCK_SR    mock_regs[3]   // +0x0C
#define MOCK_CR    mock_regs[4]   // +0x10

#define TEST_LOG_BASE  0x08020000UL
#define TEST_SIZE      (128u * 1024u)
#define WORD           4u

static uint8_t mock_flash_memory[TEST_SIZE];

void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base);

static hal_flash_t hal;

#define CR_LOCK  (1UL << 31)
#define CR_PG    (1UL <<  0)
#define CR_SER   (1UL <<  1)
#define CR_STRT  (1UL << 16)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

void setUp(void) {
    memset((void *)mock_regs, 0, sizeof(mock_regs));
    MOCK_CR = CR_LOCK;
    memset(mock_flash_memory, 0xFF, sizeof(mock_flash_memory));
    _mock_set_flash_bases((uintptr_t)mock_regs,
                          (uintptr_t)mock_flash_memory,
                          TEST_LOG_BASE);
    stm32f4_flash_init(&hal, TEST_LOG_BASE, TEST_SIZE);
}

void tearDown(void) {}

void test_unlock_writes_key2_last(void) {
    uint8_t data[WORD] = {1, 2, 3, 4};
    hal.write(TEST_LOG_BASE, data, WORD);
    TEST_ASSERT_EQUAL_HEX32(FLASH_KEY2, MOCK_KEYR);
}

void test_write_clears_pg_after_completion(void) {
    uint8_t data[WORD] = {1, 2, 3, 4};
    hal.write(TEST_LOG_BASE, data, WORD);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR & CR_PG);
}

void test_write_stores_data_in_mock_memory(void) {
    uint8_t data[WORD]     = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t readback[WORD] = {0};
    TEST_ASSERT_EQUAL_INT(0, hal.write(TEST_LOG_BASE, data, WORD));
    TEST_ASSERT_EQUAL_INT(0, hal.read (TEST_LOG_BASE, readback, WORD));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, readback, WORD);
}

void test_erase_returns_success(void) {
    TEST_ASSERT_EQUAL_INT(0, hal.erase(TEST_LOG_BASE));
}

void test_erase_clears_ser_strt_after_completion(void) {
    hal.erase(TEST_LOG_BASE);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR & (CR_SER | CR_STRT));
}

void test_read_returns_ff_on_fresh_erased_flash(void) {
    uint8_t buf[4] = {0};
    hal.read(TEST_LOG_BASE, buf, 4);
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unlock_writes_key2_last);
    RUN_TEST(test_write_clears_pg_after_completion);
    RUN_TEST(test_write_stores_data_in_mock_memory);
    RUN_TEST(test_erase_returns_success);
    RUN_TEST(test_erase_clears_ser_strt_after_completion);
    RUN_TEST(test_read_returns_ff_on_fresh_erased_flash);
    return UNITY_END();
}
