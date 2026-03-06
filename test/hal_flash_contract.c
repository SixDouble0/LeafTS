// Universal HAL flash contract tests.
//
// These tests verify the hal_flash_t guard-clause contract: every
// function must return -1 (error) for bad inputs and 0 (success) for
// a valid, correctly-initialised state.
//
// NO family-specific code is in this file.
// To add a new MCU family: write test/setup_<family>.c (15 lines)
// and add it to CMake - no changes here.
//
// Compiled and run on the host (x86).  No hardware required.

#include "../unity/unity.h"
#include "hal_flash_contract.h"
#include <stdint.h>

// ConfigurATION provided by the family-specific setup_XX.c
static HalFlashTestConfig g_cfg;
static hal_flash_t        g_hal;

void setUp(void) {
    hal_test_get_config(&g_cfg);
    g_cfg.init(&g_hal, g_cfg.base, g_cfg.size);
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// init guard-clause tests
// ---------------------------------------------------------------------------

void test_init_null_flash_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(-1, g_cfg.init(NULL, g_cfg.base, g_cfg.size));
}

void test_init_bad_base_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(-1, g_cfg.init(&g_hal, g_cfg.bad_base, g_cfg.size));
}

void test_init_zero_size_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(-1, g_cfg.init(&g_hal, g_cfg.base, 0));
}

void test_init_unaligned_size_returns_error(void) {
    // size + 1 is never a multiple of erase_unit (which is >= 2)
    TEST_ASSERT_EQUAL_INT(-1, g_cfg.init(&g_hal, g_cfg.base, g_cfg.size + 1));
}

void test_init_success_populates_hal(void) {
    TEST_ASSERT_EQUAL_INT(0, g_cfg.init(&g_hal, g_cfg.base, g_cfg.size));
    TEST_ASSERT_NOT_NULL(g_hal.read);
    TEST_ASSERT_NOT_NULL(g_hal.write);
    TEST_ASSERT_NOT_NULL(g_hal.erase);
    TEST_ASSERT_EQUAL_UINT32(g_cfg.size,       g_hal.total_size);
    TEST_ASSERT_EQUAL_UINT32(g_cfg.erase_unit, g_hal.sector_size);
    TEST_ASSERT_EQUAL_UINT32(g_cfg.write_unit, g_hal.page_size);
}

// ---------------------------------------------------------------------------
// read guard-clause tests
// ---------------------------------------------------------------------------

void test_read_null_buffer_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(-1, g_hal.read(g_cfg.base, NULL, g_cfg.write_unit));
}

void test_read_address_below_base_returns_error(void) {
    uint8_t buf[8] = {0};
    TEST_ASSERT_EQUAL_INT(-1, g_hal.read(g_cfg.base - 1, buf, 1));
}

void test_read_out_of_bounds_returns_error(void) {
    uint8_t buf[16] = {0};
    // last valid byte is at (base + size - 1); reading 8 bytes from (base+size-4) overflows
    TEST_ASSERT_EQUAL_INT(-1, g_hal.read(g_cfg.base + g_cfg.size - 4, buf, 8));
}

// ---------------------------------------------------------------------------
// write guard-clause tests
// ---------------------------------------------------------------------------

void test_write_null_buffer_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(-1, g_hal.write(g_cfg.base, NULL, g_cfg.write_unit));
}

void test_write_address_below_base_returns_error(void) {
    uint8_t buf[8] = {0};
    TEST_ASSERT_EQUAL_INT(-1, g_hal.write(g_cfg.base - g_cfg.write_unit, buf, g_cfg.write_unit));
}

void test_write_unaligned_address_returns_error(void) {
    uint8_t buf[8] = {0};
    // base is guaranteed aligned; +1 breaks alignment
    TEST_ASSERT_EQUAL_INT(-1, g_hal.write(g_cfg.base + 1, buf, g_cfg.write_unit));
}

void test_write_unaligned_size_returns_error(void) {
    uint8_t buf[16] = {0};
    // write_unit + 1 is never a valid multiple
    TEST_ASSERT_EQUAL_INT(-1, g_hal.write(g_cfg.base, buf, g_cfg.write_unit + 1));
}

void test_write_out_of_bounds_returns_error(void) {
    uint8_t buf[8] = {0};
    // start near the end so write would cross the boundary
    TEST_ASSERT_EQUAL_INT(-1, g_hal.write(g_cfg.base + g_cfg.size - 4, buf, g_cfg.write_unit));
}

// ---------------------------------------------------------------------------
// erase guard-clause tests
// ---------------------------------------------------------------------------

void test_erase_address_below_base_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(-1, g_hal.erase(g_cfg.base - g_cfg.erase_unit));
}

void test_erase_unaligned_address_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(-1, g_hal.erase(g_cfg.base + 1));
}

void test_erase_out_of_bounds_returns_error(void) {
    // exactly one erase unit past the end
    TEST_ASSERT_EQUAL_INT(-1, g_hal.erase(g_cfg.base + g_cfg.size));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_null_flash_returns_error);
    RUN_TEST(test_init_bad_base_returns_error);
    RUN_TEST(test_init_zero_size_returns_error);
    RUN_TEST(test_init_unaligned_size_returns_error);
    RUN_TEST(test_init_success_populates_hal);

    RUN_TEST(test_read_null_buffer_returns_error);
    RUN_TEST(test_read_address_below_base_returns_error);
    RUN_TEST(test_read_out_of_bounds_returns_error);

    RUN_TEST(test_write_null_buffer_returns_error);
    RUN_TEST(test_write_address_below_base_returns_error);
    RUN_TEST(test_write_unaligned_address_returns_error);
    RUN_TEST(test_write_unaligned_size_returns_error);
    RUN_TEST(test_write_out_of_bounds_returns_error);

    RUN_TEST(test_erase_address_below_base_returns_error);
    RUN_TEST(test_erase_unaligned_address_returns_error);
    RUN_TEST(test_erase_out_of_bounds_returns_error);

    return UNITY_END();
}
