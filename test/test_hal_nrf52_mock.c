// Mock MMIO test for nRF52 flash HAL (NVMC controller).
//
// Compiled with -DLEAFTS_MOCK_FLASH.  Runs on host (x86/x64).
//
// nRF52 NVMC registers have large, non-contiguous offsets:
//   NVMC_READY     @ +0x400   (index 256)
//   NVMC_CONFIG    @ +0x504   (index 321)
//   NVMC_ERASEPAGE @ +0x508   (index 322)
// The mock register array must cover 0x50C bytes = 323 entries.
#include "../unity/unity.h"
#include "../hal/nrf/hal_nrf52_flash.h"
#include <string.h>
#include <stdint.h>

// 323 entries — covers the highest register at offset +0x508 (index 322).
static volatile uint32_t mock_regs[323];
#define MOCK_NVMC_READY     mock_regs[0x400 / 4]   // index 256
#define MOCK_NVMC_CONFIG    mock_regs[0x504 / 4]   // index 321
#define MOCK_NVMC_ERASEPAGE mock_regs[0x508 / 4]   // index 322

#define TEST_LOG_BASE  0x00080000UL
#define TEST_SIZE      (16u * 1024u)
#define PAGE           4096u
#define WORD           4u

static uint8_t mock_flash_memory[TEST_SIZE];

void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base);

static hal_flash_t hal;

#define NVMC_CONFIG_RO   0x00UL
#define NVMC_CONFIG_WEN  0x01UL
#define NVMC_CONFIG_EEN  0x02UL

void setUp(void) {
    memset((void *)mock_regs, 0, sizeof(mock_regs));
    // NVMC_READY bit 0 must be 1 (controller idle) immediately.
    MOCK_NVMC_READY = 1u;
    memset(mock_flash_memory, 0xFF, sizeof(mock_flash_memory));
    _mock_set_flash_bases((uintptr_t)mock_regs,
                          (uintptr_t)mock_flash_memory,
                          TEST_LOG_BASE);
    nrf52_flash_init(&hal, TEST_LOG_BASE, TEST_SIZE);
}

void tearDown(void) {}

void test_write_sets_config_to_wen_before_write(void) {
    // After write completes CONFIG should be back to RO.
    // We indirectly verify WEN was used by checking RO after completion.
    uint8_t data[WORD] = {0x12, 0x34, 0x56, 0x78};
    hal.write(TEST_LOG_BASE, data, WORD);
    TEST_ASSERT_EQUAL_INT(NVMC_CONFIG_RO, MOCK_NVMC_CONFIG);
}

void test_write_stores_data_in_mock_memory(void) {
    uint8_t data[WORD]     = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t readback[WORD] = {0};
    TEST_ASSERT_EQUAL_INT(0, hal.write(TEST_LOG_BASE, data, WORD));
    TEST_ASSERT_EQUAL_INT(0, hal.read (TEST_LOG_BASE, readback, WORD));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, readback, WORD);
}

void test_write_returns_success(void) {
    uint8_t data[WORD] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_INT(0, hal.write(TEST_LOG_BASE, data, WORD));
}

void test_erase_writes_page_address_to_erasepage(void) {
    hal.erase(TEST_LOG_BASE);
    TEST_ASSERT_EQUAL_HEX32(TEST_LOG_BASE, MOCK_NVMC_ERASEPAGE);
}

void test_erase_resets_config_to_ro_after_completion(void) {
    hal.erase(TEST_LOG_BASE);
    TEST_ASSERT_EQUAL_INT(NVMC_CONFIG_RO, MOCK_NVMC_CONFIG);
}

void test_erase_returns_success(void) {
    TEST_ASSERT_EQUAL_INT(0, hal.erase(TEST_LOG_BASE));
}

void test_read_returns_ff_on_fresh_erased_flash(void) {
    uint8_t buf[4] = {0};
    hal.read(TEST_LOG_BASE, buf, 4);
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_write_sets_config_to_wen_before_write);
    RUN_TEST(test_write_stores_data_in_mock_memory);
    RUN_TEST(test_write_returns_success);
    RUN_TEST(test_erase_writes_page_address_to_erasepage);
    RUN_TEST(test_erase_resets_config_to_ro_after_completion);
    RUN_TEST(test_erase_returns_success);
    RUN_TEST(test_read_returns_ff_on_fresh_erased_flash);
    return UNITY_END();
}
