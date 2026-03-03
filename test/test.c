#include "../unity/unity.h"
#include "../hal/hal_flash.h"
#include "../hal/hal_vflash.h"
#include "../include/leafts.h"

// SHARED TEST FIXTURES
static hal_flash_t hal;
static leafts_db_t db;

// RUNS BEFORE EACH TEST - fresh flash and db every time
void setUp(void)
{
    vflash_init(&hal);
    leafts_init(&db, &hal, 0x0000, 4096);
}

// RUNS AFTER EACH TEST
void tearDown(void) {}

// ------------------------------------------------------------------ //

// TEST: AFTER INIT DB SHOULD BE EMPTY WITH CORRECT CAPACITY (4096 / 12 = 341)
void test_init_empty_db(void)
{
    TEST_ASSERT_EQUAL_INT(0,   db.record_count);
    TEST_ASSERT_EQUAL_INT(341, db.capacity);
    TEST_ASSERT_EQUAL_INT(0,   db.base_address);
}

// TEST: APPENDING ONE RECORD SHOULD INCREASE RECORD COUNT TO 1
void test_append_single(void)
{
    int result = leafts_append(&db, 1000, 21.75f);
    TEST_ASSERT_EQUAL_INT(LEAFTS_OK, result);
    TEST_ASSERT_EQUAL_INT(1, db.record_count);
}

// TEST: APPENDING 3 RECORDS SHOULD INCREASE RECORD COUNT TO 3
void test_append_multiple(void)
{
    leafts_append(&db, 1000, 21.75f);
    leafts_append(&db, 1060, 22.10f);
    leafts_append(&db, 1120, 22.50f);
    TEST_ASSERT_EQUAL_INT(3, db.record_count);
}

// TEST: GET_LATEST SHOULD RETURN THE LAST APPENDED RECORD WITH CORRECT DATA AND VALID MAGIC
void test_get_latest(void)
{
    leafts_append(&db, 1000, 21.75f);
    leafts_append(&db, 1060, 22.10f);
    leafts_append(&db, 1120, 22.50f);

    leafts_record_t latest_record;
    int result = leafts_get_latest(&db, &latest_record);

    TEST_ASSERT_EQUAL_INT  (LEAFTS_OK,    result);
    TEST_ASSERT_EQUAL_INT  (1120,         latest_record.timestamp);
    TEST_ASSERT_EQUAL_FLOAT(22.50f,       latest_record.value);
    TEST_ASSERT_EQUAL_INT  (LEAFTS_MAGIC, latest_record.magic);
}

// TEST: GET_BY_INDEX SHOULD RETURN CORRECT RECORD FOR EACH GIVEN INDEX
void test_get_by_index(void)
{
    leafts_append(&db, 1000, 21.75f);
    leafts_append(&db, 1060, 22.10f);
    leafts_append(&db, 1120, 22.50f);

    leafts_record_t record;

    leafts_get_by_index(&db, 0, &record);
    TEST_ASSERT_EQUAL_INT  (1000,   record.timestamp);
    TEST_ASSERT_EQUAL_FLOAT(21.75f, record.value);

    leafts_get_by_index(&db, 1, &record);
    TEST_ASSERT_EQUAL_INT(1060, record.timestamp);

    leafts_get_by_index(&db, 2, &record);
    TEST_ASSERT_EQUAL_INT(1120, record.timestamp);
}

// TEST: ERASE SHOULD CLEAR ALL RECORDS FROM FLASH AND RESET RECORD COUNT TO 0
void test_erase(void)
{
    leafts_append(&db, 1000, 21.75f);
    leafts_append(&db, 1060, 22.10f);

    int result = leafts_erase(&db);
    TEST_ASSERT_EQUAL_INT(LEAFTS_OK, result);
    TEST_ASSERT_EQUAL_INT(0,         db.record_count);

    leafts_db_t db_restored;
    leafts_init(&db_restored, &hal, 0x0000, 4096);
    TEST_ASSERT_EQUAL_INT(0, db_restored.record_count);
}

// TEST: AFTER POWER-CYCLE (RE-INIT ON SAME FLASH) RECORD COUNT SHOULD BE RESTORED FROM FLASH
void test_power_cycle(void)
{
    leafts_append(&db, 1000, 21.75f);
    leafts_append(&db, 1060, 22.10f);
    leafts_append(&db, 1120, 22.50f);

    leafts_db_t db_restored;
    int result = leafts_init(&db_restored, &hal, 0x0000, 4096);
    TEST_ASSERT_EQUAL_INT(LEAFTS_OK, result);
    TEST_ASSERT_EQUAL_INT(3,         db_restored.record_count);
}

// TEST: ALL FUNCTIONS SHOULD RETURN CORRECT ERROR CODES FOR NULL, EMPTY DB AND OUT OF BOUNDS
void test_error_codes(void)
{
    leafts_record_t record;

    TEST_ASSERT_EQUAL_INT(LEAFTS_ERR_NULL,   leafts_init(NULL, &hal, 0, 4096));
    TEST_ASSERT_EQUAL_INT(LEAFTS_ERR_NULL,   leafts_init(&db, NULL, 0, 4096));
    TEST_ASSERT_EQUAL_INT(LEAFTS_ERR_NULL,   leafts_append(NULL, 1000, 21.0f));
    TEST_ASSERT_EQUAL_INT(LEAFTS_ERR_NULL,   leafts_get_latest(NULL, &record));
    TEST_ASSERT_EQUAL_INT(LEAFTS_ERR_NULL,   leafts_get_latest(&db, NULL));
    TEST_ASSERT_EQUAL_INT(LEAFTS_ERR_EMPTY,  leafts_get_latest(&db, &record));

    leafts_append(&db, 1000, 21.0f);
    TEST_ASSERT_EQUAL_INT(LEAFTS_ERR_BOUNDS, leafts_get_by_index(&db, 99, &record));
}

// ------------------------------------------------------------------ //

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_empty_db);
    RUN_TEST(test_append_single);
    RUN_TEST(test_append_multiple);
    RUN_TEST(test_get_latest);
    RUN_TEST(test_get_by_index);
    RUN_TEST(test_erase);
    RUN_TEST(test_power_cycle);
    RUN_TEST(test_error_codes);

    return UNITY_END();
}
