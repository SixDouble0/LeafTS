#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../include/uart_handler.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Return the value at rank k (0-based) when sorted ascending — O(n^2), no malloc.
static float kth_smallest(leafts_db_t *db, uint32_t k)
{
    leafts_record_t ri, rj;
    for (uint32_t i = 0; i < db->record_count; i++) {
        if (leafts_get_by_index(db, i, &ri) != LEAFTS_OK) continue;
        uint32_t less = 0, equal = 0;
        for (uint32_t j = 0; j < db->record_count; j++) {
            if (leafts_get_by_index(db, j, &rj) != LEAFTS_OK) continue;
            if      (rj.value < ri.value) less++;
            else if (rj.value == ri.value) equal++;
        }
        if (less <= k && k < less + equal)
            return ri.value;
    }
    return 0.0f;
}

// HELPER: SEND A NULL-TERMINATED STRING OVER UART
static void uart_send_str(hal_uart_t *uart, const char *str)
{
    // SEND STRING AS BYTES 
    uart->send((const uint8_t *)str, (uint32_t)strlen(str));
}

static uint32_t auto_timestamp_now(leafts_db_t *db)
{
    time_t now = time(NULL);
    if (now > 0 && (unsigned long long)now <= 0xFFFFFFFFULL)
        return (uint32_t)now;

    if (db->record_count > 0) {
        leafts_record_t latest_record;
        if (leafts_get_latest(db, &latest_record) == LEAFTS_OK)
            return latest_record.timestamp + 1U;
    }

    return 1U;
}



// PROCESS ONE COMPLETE LINE - DISPATCH TO CORRECT leafts_* FUNCTION
// PROTOCOL:
//   insert <value>              ->  "OK\n"  or  "ERR <code>\n" (auto timestamp)
//   insert <value> <timestamp>  ->  "OK\n"  or  "ERR <code>\n" (manual timestamp)
//   select / latest             ->  "OK <timestamp> <value>\n"
//   select * / list             ->  "OK <count>\n" + one line per record
//   select count(*)             ->  "OK <count>\n" (alias: count)
//   select min(value)           ->  "OK <timestamp> <value>\n" (alias: get_min)
//   select max(value)           ->  "OK <timestamp> <value>\n" (alias: get_max)
//   select avg(value)           ->  "OK <value>\n" (alias: get_avg)
//   select * limit <n>          ->  "OK <count>\n" + last N lines (alias: get_last)
//   select * where timestamp between <from> <to> -> range list (alias: get_range)
//   get_last <n>                ->  "OK <count>\n" + last N lines
//   get_range <ts_from> <ts_to> ->  "OK <count>\n" + matching lines
//   get_min                     ->  "OK <timestamp> <value>\n"
//   get_max                     ->  "OK <timestamp> <value>\n"
//   status                      ->  "OK count=N capacity=N\n"
//   erase                       ->  "OK\n"


int uart_handler_process(const char *line, leafts_db_t *db, hal_uart_t *uart)
{
    char response[128];

    //  APPEND / INSERT 
    if (strncmp(line, "append", 6) == 0 || strncmp(line, "insert", 6) == 0)
    {
        uint32_t timestamp;
        float    value;
        char     extra;

        int has_manual_ts =
            (sscanf(line, "append %f %lu %c", &value, &timestamp, &extra) == 2) ||
            (sscanf(line, "insert %f %lu %c", &value, &timestamp, &extra) == 2);

        int has_auto_ts =
            (sscanf(line, "append %f %c", &value, &extra) == 1) ||
            (sscanf(line, "insert %f %c", &value, &extra) == 1);

        if (!has_manual_ts && !has_auto_ts)
        {
            uart_send_str(uart, "ERR bad_args\n");
            return LEAFTS_ERR_NULL;
        }

        if (!has_manual_ts)
            timestamp = auto_timestamp_now(db);

        int result = leafts_append(db, timestamp, value);

        if (result == LEAFTS_OK)
        {
            uart_send_str(uart, "OK\n");
        }
        else
        {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }

        return result;
    }

    //  LIST / SELECT *
    if (strncmp(line, "list", 4) == 0 ||
        strncmp(line, "select *", 8) == 0 ||
        strncmp(line, "select all", 10) == 0)
    {
        uint32_t sql_limit = 0;
        if (sscanf(line, "select * limit %lu", &sql_limit) == 1) {
            uint32_t n = sql_limit;
            if (n > db->record_count) n = db->record_count;

            snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)n);
            uart_send_str(uart, response);

            uint32_t start_index = db->record_count - n;
            for (uint32_t record_index = start_index; record_index < db->record_count; record_index++)
            {
                leafts_record_t record;
                if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
                {
                    snprintf(response, sizeof(response),
                             "%lu %f\n", (unsigned long)record.timestamp, record.value);
                    uart_send_str(uart, response);
                }
            }
            return LEAFTS_OK;
        }

        uint32_t ts_from_sql = 0;
        uint32_t ts_to_sql = 0;
        if (sscanf(line, "select * where timestamp between %lu %lu", &ts_from_sql, &ts_to_sql) == 2) {
            uint32_t match_count = 0;
            for (uint32_t record_index = 0; record_index < db->record_count; record_index++)
            {
                leafts_record_t record;
                if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
                {
                    if (record.timestamp >= ts_from_sql && record.timestamp <= ts_to_sql)
                        match_count++;
                }
            }

            snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)match_count);
            uart_send_str(uart, response);

            for (uint32_t record_index = 0; record_index < db->record_count; record_index++)
            {
                leafts_record_t record;
                if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
                {
                    if (record.timestamp >= ts_from_sql && record.timestamp <= ts_to_sql)
                    {
                        snprintf(response, sizeof(response),
                                 "%lu %f\n", (unsigned long)record.timestamp, record.value);
                        uart_send_str(uart, response);
                    }
                }
            }
            return LEAFTS_OK;
        }

        // SEND COUNT FIRST SO CLIENT KNOWS HOW MANY LINES TO READ
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)db->record_count);
        uart_send_str(uart, response);

        for (uint32_t record_index = 0; record_index < db->record_count; record_index++)
        {
            leafts_record_t record;

            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                snprintf(response, sizeof(response),
                         "%lu %f\n", (unsigned long)record.timestamp, record.value);
                uart_send_str(uart, response);
            }
        }

        return LEAFTS_OK;
    }

    //  LATEST / SELECT
    if (strncmp(line, "latest", 6) == 0 ||
        strcmp(line, "select") == 0 ||
        strncmp(line, "select latest", 13) == 0)
    {
        leafts_record_t record;
        int result = leafts_get_latest(db, &record);

        if (result == LEAFTS_OK)
        {
            snprintf(response, sizeof(response),
                     "OK %lu %f\n", (unsigned long)record.timestamp, record.value);
            uart_send_str(uart, response);
        }
        else
        {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }

        return result;
    }

    //  GET_LAST 
    if (strncmp(line, "get_last", 8) == 0)
    {
        uint32_t n;

        // PARSE HOW MANY LAST RECORDS TO RETURN
        if (sscanf(line, "get_last %lu", &n) != 1)
        {
            uart_send_str(uart, "ERR bad_args\n");
            return LEAFTS_ERR_NULL;
        }

        // CLAMP N TO ACTUAL RECORD COUNT - CANT RETURN MORE THAN EXISTS
        if (n > db->record_count) n = db->record_count;

        // SEND COUNT FIRST SO CLIENT KNOWS HOW MANY LINES TO READ
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)n);
        uart_send_str(uart, response);

        // START FROM (record_count - n) - THE N-TH RECORD FROM THE END
        uint32_t start_index = db->record_count - n;
        for (uint32_t record_index = start_index; record_index < db->record_count; record_index++)
        {
            leafts_record_t record;
            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                snprintf(response, sizeof(response),
                         "%lu %f\n", (unsigned long)record.timestamp, record.value);
                uart_send_str(uart, response);
            }
        }

        return LEAFTS_OK;
    }

    //  GET_RANGE 
    if (strncmp(line, "get_range", 9) == 0 || strncmp(line, "select * where timestamp between", 30) == 0)
    {
        uint32_t ts_from;
        uint32_t ts_to;

        // PARSE TIMESTAMP RANGE FROM LINE
        if (sscanf(line, "get_range %lu %lu", &ts_from, &ts_to) != 2 &&
            sscanf(line, "select * where timestamp between %lu %lu", &ts_from, &ts_to) != 2)
        {
            uart_send_str(uart, "ERR bad_args\n");
            return LEAFTS_ERR_NULL;
        }

        // FIRST PASS - COUNT HOW MANY RECORDS MATCH SO CLIENT KNOWS SIZE
        uint32_t match_count = 0;
        for (uint32_t record_index = 0; record_index < db->record_count; record_index++)
        {
            leafts_record_t record;
            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                if (record.timestamp >= ts_from && record.timestamp <= ts_to)
                    match_count++;
            }
        }

        // SEND COUNT FIRST
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)match_count);
        uart_send_str(uart, response);

        // SECOND PASS - SEND MATCHING RECORDS
        for (uint32_t record_index = 0; record_index < db->record_count; record_index++)
        {
            leafts_record_t record;
            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                if (record.timestamp >= ts_from && record.timestamp <= ts_to)
                {
                    snprintf(response, sizeof(response),
                             "%lu %f\n", (unsigned long)record.timestamp, record.value);
                    uart_send_str(uart, response);
                }
            }
        }

        return LEAFTS_OK;
    }

    //  GET_MIN 
    if (strncmp(line, "get_min", 7) == 0 || strcmp(line, "select min(value)") == 0)
    {
        if (db->record_count == 0)
        {
            uart_send_str(uart, "ERR empty\n");
            return LEAFTS_ERR_EMPTY;
        }

        // SCAN ALL RECORDS AND TRACK THE ONE WITH THE LOWEST VALUE
        leafts_record_t min_record;
        leafts_get_by_index(db, 0, &min_record);

        for (uint32_t record_index = 1; record_index < db->record_count; record_index++)
        {
            leafts_record_t record;
            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                if (record.value < min_record.value)
                    min_record = record;
            }
        }

        snprintf(response, sizeof(response),
                 "OK %lu %f\n", (unsigned long)min_record.timestamp, min_record.value);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  GET_MAX 
    if (strncmp(line, "get_max", 7) == 0 || strcmp(line, "select max(value)") == 0)
    {
        if (db->record_count == 0)
        {
            uart_send_str(uart, "ERR empty\n");
            return LEAFTS_ERR_EMPTY;
        }

        // SCAN ALL RECORDS AND TRACK THE ONE WITH THE HIGHEST VALUE
        leafts_record_t max_record;
        leafts_get_by_index(db, 0, &max_record);

        for (uint32_t record_index = 1; record_index < db->record_count; record_index++)
        {
            leafts_record_t record;
            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                if (record.value > max_record.value)
                    max_record = record;
            }
        }

        snprintf(response, sizeof(response),
                 "OK %lu %f\n", (unsigned long)max_record.timestamp, max_record.value);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  STATUS 
    if (strncmp(line, "status", 6) == 0)
    {
        snprintf(response, sizeof(response),
                 "OK count=%lu capacity=%lu\n",
                 (unsigned long)db->record_count,
                 (unsigned long)db->capacity);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  ERASE 
    if (strncmp(line, "erase", 5) == 0 ||
        strcmp(line, "delete from leafts") == 0 ||
        strcmp(line, "truncate table leafts") == 0)
    {
        int result = leafts_erase(db);

        if (result == LEAFTS_OK)
        {
            uart_send_str(uart, "OK\n");
        }
        else
        {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }

        return result;
    }

    //  GET_FIRST
    if (strncmp(line, "get_first", 9) == 0)
    {
        if (db->record_count == 0) { uart_send_str(uart, "ERR empty\n"); return LEAFTS_ERR_EMPTY; }
        leafts_record_t record;
        int result = leafts_get_by_index(db, 0, &record);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK %lu %f\n",
                     (unsigned long)record.timestamp, record.value);
            uart_send_str(uart, response);
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    //  GET_AVG
    if (strncmp(line, "get_avg_range", 13) == 0)
    {
        uint32_t ts_from, ts_to;
        if (sscanf(line, "get_avg_range %lu %lu", &ts_from, &ts_to) != 2) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        double sum = 0.0; uint32_t n = 0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK &&
                r.timestamp >= ts_from && r.timestamp <= ts_to)
            { sum += r.value; n++; }
        }
        if (n == 0) { uart_send_str(uart, "ERR empty\n"); return LEAFTS_ERR_EMPTY; }
        snprintf(response, sizeof(response), "OK %f\n", (float)(sum / n));
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    if (strncmp(line, "get_avg", 7) == 0 || strcmp(line, "select avg(value)") == 0)
    {
        if (db->record_count == 0) { uart_send_str(uart, "ERR empty\n"); return LEAFTS_ERR_EMPTY; }
        double sum = 0.0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK) sum += r.value;
        }
        snprintf(response, sizeof(response), "OK %f\n", (float)(sum / db->record_count));
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  COUNT
    if (strncmp(line, "count", 5) == 0 || strcmp(line, "select count(*)") == 0)
    {
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)db->record_count);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  GET_BY_INDEX
    if (strncmp(line, "get_by_index", 12) == 0)
    {
        uint32_t idx;
        if (sscanf(line, "get_by_index %lu", &idx) != 1) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        leafts_record_t r;
        int result = leafts_get_by_index(db, idx, &r);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK %lu %f\n",
                     (unsigned long)r.timestamp, r.value);
            uart_send_str(uart, response);
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    //  GET_STDDEV
    if (strncmp(line, "get_stddev", 10) == 0)
    {
        if (db->record_count < 2) { uart_send_str(uart, "ERR not_enough\n"); return LEAFTS_ERR_EMPTY; }
        double sum = 0.0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK) sum += r.value;
        }
        double mean = sum / db->record_count;
        double sq_sum = 0.0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK) {
                double d = r.value - mean; sq_sum += d * d;
            }
        }
        snprintf(response, sizeof(response), "OK %f\n",
                 (float)sqrt(sq_sum / (db->record_count - 1)));
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  GET_SUM
    if (strncmp(line, "get_sum", 7) == 0)
    {
        double sum = 0.0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK) sum += r.value;
        }
        snprintf(response, sizeof(response), "OK %f\n", (float)sum);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  GET_COUNT_RANGE
    if (strncmp(line, "get_count_range", 15) == 0)
    {
        uint32_t ts_from, ts_to;
        if (sscanf(line, "get_count_range %lu %lu", &ts_from, &ts_to) != 2) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        uint32_t n = 0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK &&
                r.timestamp >= ts_from && r.timestamp <= ts_to) n++;
        }
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)n);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  GET_NTH_LAST
    if (strncmp(line, "get_nth_last", 12) == 0)
    {
        uint32_t n;
        if (sscanf(line, "get_nth_last %lu", &n) != 1) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        if (n == 0 || n > db->record_count) {
            uart_send_str(uart, "ERR bounds\n"); return LEAFTS_ERR_BOUNDS;
        }
        leafts_record_t r;
        int result = leafts_get_by_index(db, db->record_count - n, &r);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK %lu %f\n",
                     (unsigned long)r.timestamp, r.value);
            uart_send_str(uart, response);
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    //  GET_ABOVE
    if (strncmp(line, "get_above", 9) == 0)
    {
        float threshold;
        if (sscanf(line, "get_above %f", &threshold) != 1) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        uint32_t n = 0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK && r.value > threshold) n++;
        }
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)n);
        uart_send_str(uart, response);
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK && r.value > threshold) {
                snprintf(response, sizeof(response), "%lu %f\n",
                         (unsigned long)r.timestamp, r.value);
                uart_send_str(uart, response);
            }
        }
        return LEAFTS_OK;
    }

    //  GET_BELOW
    if (strncmp(line, "get_below", 9) == 0)
    {
        float threshold;
        if (sscanf(line, "get_below %f", &threshold) != 1) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        uint32_t n = 0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK && r.value < threshold) n++;
        }
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)n);
        uart_send_str(uart, response);
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK && r.value < threshold) {
                snprintf(response, sizeof(response), "%lu %f\n",
                         (unsigned long)r.timestamp, r.value);
                uart_send_str(uart, response);
            }
        }
        return LEAFTS_OK;
    }

    //  GET_BETWEEN
    if (strncmp(line, "get_between", 11) == 0)
    {
        float v1, v2;
        if (sscanf(line, "get_between %f %f", &v1, &v2) != 2) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        uint32_t n = 0;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK &&
                r.value >= v1 && r.value <= v2) n++;
        }
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)n);
        uart_send_str(uart, response);
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK &&
                r.value >= v1 && r.value <= v2) {
                snprintf(response, sizeof(response), "%lu %f\n",
                         (unsigned long)r.timestamp, r.value);
                uart_send_str(uart, response);
            }
        }
        return LEAFTS_OK;
    }

    //  GET_MIN_RANGE / GET_MAX_RANGE
    if (strncmp(line, "get_min_range", 13) == 0 || strncmp(line, "get_max_range", 13) == 0)
    {
        int is_min = (line[5] == 'i'); /* get_m[i]n_range vs get_m[a]x_range */
        uint32_t ts_from, ts_to;
        if (sscanf(line + (is_min ? 13 : 13), " %lu %lu", &ts_from, &ts_to) != 2) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        int found = 0;
        leafts_record_t best;
        for (uint32_t i = 0; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK &&
                r.timestamp >= ts_from && r.timestamp <= ts_to) {
                if (!found || (is_min ? r.value < best.value : r.value > best.value)) {
                    best = r; found = 1;
                }
            }
        }
        if (!found) { uart_send_str(uart, "ERR empty\n"); return LEAFTS_ERR_EMPTY; }
        snprintf(response, sizeof(response), "OK %lu %f\n",
                 (unsigned long)best.timestamp, best.value);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  GET_LATEST_N (alias for get_last)
    if (strncmp(line, "get_latest_n", 12) == 0)
    {
        uint32_t n;
        if (sscanf(line, "get_latest_n %lu", &n) != 1) {
            uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL;
        }
        if (n > db->record_count) n = db->record_count;
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)n);
        uart_send_str(uart, response);
        uint32_t start = db->record_count - n;
        for (uint32_t i = start; i < db->record_count; i++) {
            leafts_record_t r;
            if (leafts_get_by_index(db, i, &r) == LEAFTS_OK) {
                snprintf(response, sizeof(response), "%lu %f\n",
                         (unsigned long)r.timestamp, r.value);
                uart_send_str(uart, response);
            }
        }
        return LEAFTS_OK;
    }

    //  GET_MEDIAN
    if (strncmp(line, "get_median", 10) == 0)
    {
        if (db->record_count == 0) { uart_send_str(uart, "ERR empty\n"); return LEAFTS_ERR_EMPTY; }
        uint32_t n = db->record_count;
        float median;
        if (n % 2 == 1) {
            median = kth_smallest(db, n / 2);
        } else {
            median = (kth_smallest(db, n / 2 - 1) + kth_smallest(db, n / 2)) * 0.5f;
        }
        snprintf(response, sizeof(response), "OK %f\n", median);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    //  UNKNOWN COMMAND 
    uart_send_str(uart, "ERR unknown_cmd\n");
    return LEAFTS_ERR_NULL;
}
