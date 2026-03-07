// Mock MMIO test for STM32F1 flash HAL.
//
// Compiled with -DLEAFTS_MOCK_FLASH.  Runs on host (x86/x64).
//
// F1/F3-style register layout: KEYR@+0x04, SR@+0x0C, CR@+0x10, AR@+0x14.
// CR_STRT is bit 6 and CR_LOCK is bit 7 — different from L4 / F4!
// Write unit is 2 bytes (half-word).
// stm32f1_flash_init takes a 4th parameter: page_bytes.
#include "../unity/unity.h"
#include "../hal/stm32/hal_stm32f1_flash.h"
#include <string.h>
#include <stdint.h>

// 6 entries covers AR at offset +0x14 (index 5).
static volatile uint32_t mock_regs[6];
#define MOCK_KEYR  mock_regs[1]   // +0x04
#define MOCK_SR    mock_regs[3]   // +0x0C
#define MOCK_CR    mock_regs[4]   // +0x10
#define MOCK_AR    mock_regs[5]   // +0x14

#define TEST_LOG_BASE  0x08010000UL
#define TEST_SIZE      (64u * 1024u)
#define PAGE           2048u
#define HWORD          2u

static uint8_t mock_flash_memory[TEST_SIZE];

void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base);

static hal_flash_t hal;

// F1 CR bit positions differ from L4/F4.
#define CR_PG    (1UL << 0)
#define CR_PER   (1UL << 1)
#define CR_STRT  (1UL << 6)
#define CR_LOCK  (1UL << 7)

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

void setUp(void) {
    memset((void *)mock_regs, 0, sizeof(mock_regs));
    MOCK_CR = CR_LOCK;
    memset(mock_flash_memory, 0xFF, sizeof(mock_flash_memory));
    _mock_set_flash_bases((uintptr_t)mock_regs,
                          (uintptr_t)mock_flash_memory,
                          TEST_LOG_BASE);
    // stm32f1_flash_init has 4 parameters; last one is page size in bytes.
    stm32f1_flash_init(&hal, TEST_LOG_BASE, TEST_SIZE, PAGE);
}

void tearDown(void) {}

void test_unlock_writes_key2_last(void) {
    uint8_t data[HWORD] = {1, 2};
    hal.write(TEST_LOG_BASE, data, HWORD);
    TEST_ASSERT_EQUAL_HEX32(FLASH_KEY2, MOCK_KEYR);
}

void test_write_clears_pg_after_completion(void) {
    uint8_t data[HWORD] = {1, 2};
    hal.write(TEST_LOG_BASE, data, HWORD);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR & CR_PG);
}

void test_write_stores_data_in_mock_memory(void) {
    uint8_t data[HWORD]     = {0xCA, 0xFE};
    uint8_t readback[HWORD] = {0};
    TEST_ASSERT_EQUAL_INT(0, hal.write(TEST_LOG_BASE, data, HWORD));
    TEST_ASSERT_EQUAL_INT(0, hal.read (TEST_LOG_BASE, readback, HWORD));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, readback, HWORD);
}

void test_erase_sets_ar_to_page_address(void) {
    hal.erase(TEST_LOG_BASE);
    TEST_ASSERT_EQUAL_HEX32(TEST_LOG_BASE, MOCK_AR);
}

void test_erase_returns_success(void) {
    TEST_ASSERT_EQUAL_INT(0, hal.erase(TEST_LOG_BASE));
}

void test_erase_clears_per_strt_after_completion(void) {
    hal.erase(TEST_LOG_BASE);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR & (CR_PER | CR_STRT));
}

void test_read_returns_ff_on_fresh_erased_flash(void) {
    uint8_t buf[2] = {0};
    hal.read(TEST_LOG_BASE, buf, 2);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buf[1]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unlock_writes_key2_last);
    RUN_TEST(test_write_clears_pg_after_completion);
    RUN_TEST(test_write_stores_data_in_mock_memory);
    RUN_TEST(test_erase_sets_ar_to_page_address);
    RUN_TEST(test_erase_returns_success);
    RUN_TEST(test_erase_clears_per_strt_after_completion);
    RUN_TEST(test_read_returns_ff_on_fresh_erased_flash);
    return UNITY_END();
}
