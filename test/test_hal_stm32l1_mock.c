// Mock MMIO test for STM32L1 flash HAL.
//
// Compiled with -DLEAFTS_MOCK_FLASH.  Runs on host (x86/x64).
//
// L1 uses the PECR/PEKEY/PRGKEY unlock scheme (different from all other STM32).
// Register offsets: PECR@+0x04, PEKEYR@+0x0C, PRGKEYR@+0x10, SR@+0x18.
// Write unit: 4 bytes (word).  Erase unit: 256 bytes (page).
// IMPORTANT: STM32L1 erased state is 0x00, not 0xFF.
#include "../unity/unity.h"
#include "../hal/stm32/hal_stm32l1_flash.h"
#include <string.h>
#include <stdint.h>

// 7 entries covers SR at offset +0x18 (index 6).
static volatile uint32_t mock_regs[7];
#define MOCK_PECR    mock_regs[1]   // +0x04
#define MOCK_PEKEYR  mock_regs[3]   // +0x0C
#define MOCK_PRGKEYR mock_regs[4]   // +0x10
#define MOCK_SR      mock_regs[6]   // +0x18

#define TEST_LOG_BASE  0x08010000UL
#define TEST_SIZE      (16u * 1024u)
#define PAGE           256u
#define WORD           4u

// STM32L1 erased state is 0x00 (EVM technology, erase writes zeros).
static uint8_t mock_flash_memory[TEST_SIZE];

void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base);

static hal_flash_t hal;

#define PECR_PELOCK   (1UL <<  0)
#define PECR_PRGLOCK  (1UL <<  1)
#define PECR_PROG     (1UL <<  3)
#define PECR_ERASE    (1UL <<  9)

#define PEKEY1   0x89ABCDEFUL
#define PEKEY2   0x02030405UL
#define PRGKEY1  0x8C9DAEBFUL
#define PRGKEY2  0x13141516UL

void setUp(void) {
    memset((void *)mock_regs, 0, sizeof(mock_regs));
    // Both locks start engaged.
    MOCK_PECR = PECR_PELOCK | PECR_PRGLOCK;
    // L1 erased state is 0x00.
    memset(mock_flash_memory, 0x00, sizeof(mock_flash_memory));
    _mock_set_flash_bases((uintptr_t)mock_regs,
                          (uintptr_t)mock_flash_memory,
                          TEST_LOG_BASE);
    stm32l1_flash_init(&hal, TEST_LOG_BASE, TEST_SIZE);
}

void tearDown(void) {}

void test_unlock_writes_prgkey2_last(void) {
    uint8_t data[WORD] = {1, 2, 3, 4};
    hal.write(TEST_LOG_BASE, data, WORD);
    // PRGKEYR should have received PRGKEY2 as the second write.
    TEST_ASSERT_EQUAL_HEX32(PRGKEY2, MOCK_PRGKEYR);
}

void test_write_clears_prog_after_completion(void) {
    uint8_t data[WORD] = {1, 2, 3, 4};
    hal.write(TEST_LOG_BASE, data, WORD);
    TEST_ASSERT_EQUAL_INT(0, MOCK_PECR & PECR_PROG);
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

void test_erase_clears_prog_erase_after_completion(void) {
    hal.erase(TEST_LOG_BASE);
    TEST_ASSERT_EQUAL_INT(0, MOCK_PECR & (PECR_PROG | PECR_ERASE));
}

void test_read_returns_zero_on_fresh_erased_flash(void) {
    // STM32L1 erased pages read as 0x00, not 0xFF.
    uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    hal.read(TEST_LOG_BASE, buf, 4);
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_HEX8(0x00, buf[i]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unlock_writes_prgkey2_last);
    RUN_TEST(test_write_clears_prog_after_completion);
    RUN_TEST(test_write_stores_data_in_mock_memory);
    RUN_TEST(test_erase_returns_success);
    RUN_TEST(test_erase_clears_prog_erase_after_completion);
    RUN_TEST(test_read_returns_zero_on_fresh_erased_flash);
    return UNITY_END();
}
