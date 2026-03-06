// Mock MMIO test for STM32L4 flash HAL.
//
// Compiled with -DLEAFTS_MOCK_FLASH which makes the driver's register
// accesses and memory accesses point to local RAM buffers instead of
// hardware addresses.  Runs entirely on x86 / host machine.
//
// What this tests:
//   - unlock sequence actually writes KEY1 then KEY2 to KEYR
//   - write leaves CR with PG cleared after completion
//   - write stores the data in the backing memory (verifiable via read)
//   - erase cleans up CR (PER, STRT, PNB cleared after operation)
//   - read returns 0xFF on freshly erased "flash"
#include "../unity/unity.h"
#include "../hal/hal_stm32l4_flash.h"
#include <string.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Mock register block
// Offsets used by the driver (RM0394 §3.7):
//   base + 0x08 = KEYR  → index 2
//   base + 0x10 = SR    → index 4
//   base + 0x14 = CR    → index 5
// ---------------------------------------------------------------------------
static volatile uint32_t mock_regs[6];   // 6 x uint32 = 0x18 bytes, covers CR at +0x14
#define MOCK_KEYR  mock_regs[2]   // offset 0x08
#define MOCK_SR    mock_regs[4]   // offset 0x10
#define MOCK_CR    mock_regs[5]   // offset 0x14

// ---------------------------------------------------------------------------
// Mock flash memory (backing store)
// ---------------------------------------------------------------------------
#define TEST_LOG_BASE     0x08000000UL
#define TEST_BASE_UINT32  0x08000000UL   // LeafTS region starts at log base
#define TEST_SIZE         (128u * 1024u)
#define PAGE              2048u
#define DWORD             8u

static uint8_t mock_flash_memory[TEST_SIZE];

// Driver's test-only hook (only exists when LEAFTS_MOCK_FLASH is defined)
void _mock_set_flash_bases(uintptr_t reg_base, uintptr_t mem_base, uint32_t log_base);

static hal_flash_t hal;

// CR / SR bit constants (mirrors what the driver uses internally)
#define CR_LOCK  (1UL << 31)
#define CR_PG    (1UL <<  0)
#define CR_PER   (1UL <<  1)
#define CR_STRT  (1UL << 16)
#define CR_PNB_POS  3

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

void setUp(void) {
    // Reset mock registers: BSY=0, no errors, CR=LOCK (realistic start state)
    memset((void *)mock_regs, 0, sizeof(mock_regs));
    MOCK_CR = CR_LOCK;

    // Erased flash is all 0xFF
    memset(mock_flash_memory, 0xFF, sizeof(mock_flash_memory));

    // Wire driver to mocks
    _mock_set_flash_bases((uintptr_t)mock_regs,
                          (uintptr_t)mock_flash_memory,
                          TEST_LOG_BASE);

    stm32l4_flash_init(&hal, TEST_BASE_UINT32, TEST_SIZE);
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Unlock sequence
// ---------------------------------------------------------------------------

void test_unlock_writes_key2_last(void) {
    // Mock CR starts LOCKED → flash_unlock() unconditionally writes KEY1, KEY2.
    // KEY2 is the last write, so KEYR must hold KEY2 after any HW operation.
    uint8_t data[DWORD] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    hal.write(TEST_BASE_UINT32, data, DWORD);
    TEST_ASSERT_EQUAL_HEX32(FLASH_KEY2, MOCK_KEYR);
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

void test_write_clears_pg_after_completion(void) {
    uint8_t data[DWORD] = {1, 2, 3, 4, 5, 6, 7, 8};
    hal.write(TEST_BASE_UINT32, data, DWORD);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR & CR_PG);
}

void test_write_stores_data_in_mock_memory(void) {
    uint8_t data[DWORD]    = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    uint8_t readback[DWORD] = {0};

    TEST_ASSERT_EQUAL_INT(0, hal.write(TEST_BASE_UINT32, data, DWORD));
    TEST_ASSERT_EQUAL_INT(0, hal.read(TEST_BASE_UINT32, readback, DWORD));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, readback, DWORD);
}

void test_write_multiple_dwords_stored_correctly(void) {
    uint8_t data[32];
    for (int i = 0; i < 32; i++) data[i] = (uint8_t)(i * 3);
    uint8_t readback[32] = {0};

    TEST_ASSERT_EQUAL_INT(0, hal.write(TEST_BASE_UINT32, data, 32));
    TEST_ASSERT_EQUAL_INT(0, hal.read (TEST_BASE_UINT32, readback, 32));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, readback, 32);
}

void test_write_at_last_valid_dword_succeeds(void) {
    uint32_t last = TEST_BASE_UINT32 + TEST_SIZE - DWORD;
    uint8_t data[DWORD] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    TEST_ASSERT_EQUAL_INT(0, hal.write(last, data, DWORD));
}

// ---------------------------------------------------------------------------
// Erase
// ---------------------------------------------------------------------------

void test_erase_returns_success(void) {
    TEST_ASSERT_EQUAL_INT(0, hal.erase(TEST_BASE_UINT32));
}

void test_erase_clears_per_and_strt_after_completion(void) {
    hal.erase(TEST_BASE_UINT32);
    TEST_ASSERT_EQUAL_INT(0, MOCK_CR & (CR_PER | CR_STRT));
}

void test_erase_clears_pnb_bits_after_completion(void) {
    hal.erase(TEST_BASE_UINT32 + PAGE);   // page 1
    TEST_ASSERT_EQUAL_INT(0, (MOCK_CR >> CR_PNB_POS) & 0x7F);
}

// ---------------------------------------------------------------------------
// Read
// ---------------------------------------------------------------------------

void test_read_returns_ff_on_fresh_erased_flash(void) {
    uint8_t buf[16] = {0};
    hal.read(TEST_BASE_UINT32, buf, 16);
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
    }
}

void test_read_at_offset_returns_correct_data(void) {
    uint32_t addr = TEST_BASE_UINT32 + PAGE * 2;  // 2 pages in
    uint8_t  data[DWORD] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    uint8_t  buf [DWORD] = {0};

    hal.write(addr, data, DWORD);
    hal.read (addr, buf,  DWORD);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, buf, DWORD);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_unlock_writes_key2_last);

    RUN_TEST(test_write_clears_pg_after_completion);
    RUN_TEST(test_write_stores_data_in_mock_memory);
    RUN_TEST(test_write_multiple_dwords_stored_correctly);
    RUN_TEST(test_write_at_last_valid_dword_succeeds);

    RUN_TEST(test_erase_returns_success);
    RUN_TEST(test_erase_clears_per_and_strt_after_completion);
    RUN_TEST(test_erase_clears_pnb_bits_after_completion);

    RUN_TEST(test_read_returns_ff_on_fresh_erased_flash);
    RUN_TEST(test_read_at_offset_returns_correct_data);

    return UNITY_END();
}
