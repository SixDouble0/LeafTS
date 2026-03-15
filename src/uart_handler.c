#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static int parse_timestamp_token(const char *token, uint32_t *out_ts)
{
    if (!token || !out_ts) return 0;

    unsigned long raw_ts = 0;
    char tail = '\0';
    if (sscanf(token, "%lu%c", &raw_ts, &tail) == 1) {
        *out_ts = (uint32_t)raw_ts;
        return 1;
    }

    unsigned int dd = 1, mm = 1, yyyy = 1970, hh = 0, min = 0, ss = 0;
    int parts = sscanf(token, "%u.%u.%u.%u.%u.%u", &dd, &mm, &yyyy, &hh, &min, &ss);
    if (parts < 3) return 0;

    struct tm tm_value;
    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_mday = (int)dd;
    tm_value.tm_mon = (int)mm - 1;
    tm_value.tm_year = (int)yyyy - 1900;
    tm_value.tm_hour = (int)hh;
    tm_value.tm_min = (int)min;
    tm_value.tm_sec = (int)ss;
    tm_value.tm_isdst = -1;

    time_t converted = mktime(&tm_value);
    if (converted < 0 || (unsigned long long)converted > 0xFFFFFFFFULL) return 0;
    *out_ts = (uint32_t)converted;
    return 1;
}

static int erase_n_smallest_records(leafts_db_t *db, uint32_t n, uint32_t *removed_out)
{
    if (!db || !removed_out) return LEAFTS_ERR_NULL;
    if (db->record_count == 0) return LEAFTS_ERR_EMPTY;
    if (n == 0) return LEAFTS_ERR_NULL;

    uint32_t total = db->record_count;
    if (n >= total) {
        int r = leafts_erase(db);
        if (r == LEAFTS_OK) *removed_out = total;
        return r;
    }

    leafts_record_t *records = (leafts_record_t *)malloc((size_t)total * sizeof(leafts_record_t));
    uint8_t *remove_mask = (uint8_t *)calloc(total, sizeof(uint8_t));
    if (!records || !remove_mask) {
        free(records);
        free(remove_mask);
        return LEAFTS_ERR_NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        if (leafts_get_by_index(db, i, &records[i]) != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return LEAFTS_ERR_HAL;
        }
    }

    for (uint32_t k = 0; k < n; k++) {
        uint32_t min_idx = total;
        float min_val = 0.0f;
        for (uint32_t i = 0; i < total; i++) {
            if (remove_mask[i]) continue;
            if (min_idx == total || records[i].value < min_val) {
                min_idx = i;
                min_val = records[i].value;
            }
        }
        if (min_idx == total) break;
        remove_mask[min_idx] = 1U;
    }

    int r = leafts_erase(db);
    if (r != LEAFTS_OK) {
        free(records);
        free(remove_mask);
        return r;
    }

    uint32_t removed = 0;
    for (uint32_t i = 0; i < total; i++) {
        if (remove_mask[i]) {
            removed++;
            continue;
        }
        r = leafts_append(db, records[i].timestamp, records[i].value);
        if (r != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return r;
        }
    }

    free(records);
    free(remove_mask);
    *removed_out = removed;
    return LEAFTS_OK;
}

static int erase_n_largest_records(leafts_db_t *db, uint32_t n, uint32_t *removed_out)
{
    if (!db || !removed_out) return LEAFTS_ERR_NULL;
    if (db->record_count == 0) return LEAFTS_ERR_EMPTY;
    if (n == 0) return LEAFTS_ERR_NULL;

    uint32_t total = db->record_count;
    if (n >= total) {
        int r = leafts_erase(db);
        if (r == LEAFTS_OK) *removed_out = total;
        return r;
    }

    leafts_record_t *records = (leafts_record_t *)malloc((size_t)total * sizeof(leafts_record_t));
    uint8_t *remove_mask = (uint8_t *)calloc(total, sizeof(uint8_t));
    if (!records || !remove_mask) {
        free(records);
        free(remove_mask);
        return LEAFTS_ERR_NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        if (leafts_get_by_index(db, i, &records[i]) != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return LEAFTS_ERR_HAL;
        }
    }

    for (uint32_t k = 0; k < n; k++) {
        uint32_t max_idx = total;
        float max_val = 0.0f;
        for (uint32_t i = 0; i < total; i++) {
            if (remove_mask[i]) continue;
            if (max_idx == total || records[i].value > max_val) {
                max_idx = i;
                max_val = records[i].value;
            }
        }
        if (max_idx == total) break;
        remove_mask[max_idx] = 1U;
    }

    int r = leafts_erase(db);
    if (r != LEAFTS_OK) {
        free(records);
        free(remove_mask);
        return r;
    }

    uint32_t removed = 0;
    for (uint32_t i = 0; i < total; i++) {
        if (remove_mask[i]) {
            removed++;
            continue;
        }
        r = leafts_append(db, records[i].timestamp, records[i].value);
        if (r != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return r;
        }
    }

    free(records);
    free(remove_mask);
    *removed_out = removed;
    return LEAFTS_OK;
}

static int erase_records_in_timestamp_range(leafts_db_t *db, uint32_t ts_from, uint32_t ts_to, uint32_t *removed_out)
{
    if (!db || !removed_out) return LEAFTS_ERR_NULL;
    if (db->record_count == 0) return LEAFTS_ERR_EMPTY;

    uint32_t total = db->record_count;
    leafts_record_t *records = (leafts_record_t *)malloc((size_t)total * sizeof(leafts_record_t));
    uint8_t *remove_mask = (uint8_t *)calloc(total, sizeof(uint8_t));
    if (!records || !remove_mask) {
        free(records);
        free(remove_mask);
        return LEAFTS_ERR_NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        if (leafts_get_by_index(db, i, &records[i]) != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return LEAFTS_ERR_HAL;
        }
        if (records[i].timestamp >= ts_from && records[i].timestamp <= ts_to)
            remove_mask[i] = 1U;
    }

    int r = leafts_erase(db);
    if (r != LEAFTS_OK) {
        free(records);
        free(remove_mask);
        return r;
    }

    uint32_t removed = 0;
    for (uint32_t i = 0; i < total; i++) {
        if (remove_mask[i]) {
            removed++;
            continue;
        }
        r = leafts_append(db, records[i].timestamp, records[i].value);
        if (r != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return r;
        }
    }

    free(records);
    free(remove_mask);
    *removed_out = removed;
    return LEAFTS_OK;
}

static int erase_records_by_exact_value(leafts_db_t *db, float target, uint32_t *removed_out)
{
    if (!db || !removed_out) return LEAFTS_ERR_NULL;
    if (db->record_count == 0) return LEAFTS_ERR_EMPTY;

    uint32_t total = db->record_count;
    leafts_record_t *records = (leafts_record_t *)malloc((size_t)total * sizeof(leafts_record_t));
    uint8_t *remove_mask = (uint8_t *)calloc(total, sizeof(uint8_t));
    if (!records || !remove_mask) {
        free(records);
        free(remove_mask);
        return LEAFTS_ERR_NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        if (leafts_get_by_index(db, i, &records[i]) != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return LEAFTS_ERR_HAL;
        }
        if (fabsf(records[i].value - target) <= 1e-6f)
            remove_mask[i] = 1U;
    }

    int r = leafts_erase(db);
    if (r != LEAFTS_OK) {
        free(records);
        free(remove_mask);
        return r;
    }

    uint32_t removed = 0;
    for (uint32_t i = 0; i < total; i++) {
        if (remove_mask[i]) {
            removed++;
            continue;
        }
        r = leafts_append(db, records[i].timestamp, records[i].value);
        if (r != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return r;
        }
    }

    free(records);
    free(remove_mask);
    *removed_out = removed;
    return LEAFTS_OK;
}

static int erase_extreme_from_last_n(leafts_db_t *db, uint32_t n, int pick_max, uint32_t *removed_out)
{
    if (!db || !removed_out) return LEAFTS_ERR_NULL;
    if (db->record_count == 0) return LEAFTS_ERR_EMPTY;
    if (n == 0) return LEAFTS_ERR_NULL;

    uint32_t total = db->record_count;
    if (n > total) n = total;

    leafts_record_t *records = (leafts_record_t *)malloc((size_t)total * sizeof(leafts_record_t));
    uint8_t *remove_mask = (uint8_t *)calloc(total, sizeof(uint8_t));
    if (!records || !remove_mask) {
        free(records);
        free(remove_mask);
        return LEAFTS_ERR_NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        if (leafts_get_by_index(db, i, &records[i]) != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return LEAFTS_ERR_HAL;
        }
    }

    uint32_t start = total - n;
    uint32_t target = start;
    float extreme = records[start].value;
    for (uint32_t i = start + 1; i < total; i++) {
        if ((pick_max && records[i].value > extreme) ||
            (!pick_max && records[i].value < extreme)) {
            extreme = records[i].value;
            target = i;
        }
    }
    remove_mask[target] = 1U;

    int r = leafts_erase(db);
    if (r != LEAFTS_OK) {
        free(records);
        free(remove_mask);
        return r;
    }

    uint32_t removed = 0;
    for (uint32_t i = 0; i < total; i++) {
        if (remove_mask[i]) {
            removed++;
            continue;
        }
        r = leafts_append(db, records[i].timestamp, records[i].value);
        if (r != LEAFTS_OK) {
            free(records);
            free(remove_mask);
            return r;
        }
    }

    free(records);
    free(remove_mask);
    *removed_out = removed;
    return LEAFTS_OK;
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
//   delete from leafts order by value asc limit <n> -> remove N smallest values
//   erase min <n>               ->  alias for deleting N smallest values
//   delete from leafts order by value desc limit <n> -> remove N largest values
//   erase max <n>               ->  alias for deleting N largest values
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
        uint32_t timestamp = 0;
        float    value = 0.0f;
        int      is_text = 0;
        char     text_val[5] = {0};
        char     val_token[64] = {0};
        char     ts_token[64] = {0};
        char     extra;
        
        int parsed = sscanf(line, "append %63s %63s %c", val_token, ts_token, &extra);
        if (parsed < 1) {
            parsed = sscanf(line, "insert %63s %63s %c", val_token, ts_token, &extra);
        }

        if (parsed == 0) {
            snprintf(response, sizeof(response), "ERR bad_args (parsed=%d, p1=\"%s\", p2=\"%s\")\n", parsed, val_token, ts_token);
            uart_send_str(uart, response);
            return LEAFTS_ERR_NULL;
        }

        char extra_f;
        if (sscanf(val_token, "%f%c", &value, &extra_f) != 1) {
            is_text = 1;
            strncpy(text_val, val_token, 4);
            text_val[4] = '\0';
        }

        int has_manual_ts = (parsed == 2);

        if (has_manual_ts) {
            if (!parse_timestamp_token(ts_token, &timestamp)) {
                uart_send_str(uart, "ERR bad_args\n");
                return LEAFTS_ERR_NULL;
            }
        } else {
            timestamp = auto_timestamp_now(db);
        }

        int result;
        if (is_text) {
            result = leafts_append_text(db, timestamp, text_val);
        } else {
            result = leafts_append(db, timestamp, value);
        }

        if (result == LEAFTS_OK) {
            uart_send_str(uart, "OK\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    //  LIST (legacy)
    if (strncmp(line, "list", 4) == 0 || strncmp(line, "select all", 10) == 0)
    {
        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)db->record_count);
        uart_send_str(uart, response);

        for (uint32_t record_index = 0; record_index < db->record_count; record_index++)
        {
            leafts_record_t record;

            if (leafts_get_by_index(db, record_index, &record) == LEAFTS_OK)
            {
                if (record.magic == LEAFTS_MAGIC_TEXT) {
                    snprintf(response, sizeof(response),
                             "%lu %.4s\n", (unsigned long)record.timestamp, record.text);
                } else {
                    snprintf(response, sizeof(response),
                             "%lu %f\n", (unsigned long)record.timestamp, record.value);
                }
                uart_send_str(uart, response);
            }
        }

        return LEAFTS_OK;
    }

    //  SELECT * with stacked filters/aggregates
    if (strncmp(line, "select *", 8) == 0 || strncmp(line, "select limit ", 13) == 0 || strncmp(line, "select ts(", 10) == 0 ||
        strcmp(line, "select min") == 0 || strcmp(line, "select max") == 0 || strcmp(line, "select avg") == 0 || strcmp(line, "select count") == 0)
    {
        uint32_t ts_from_sql = 0, ts_to_sql = 0;
        char ts_from_tok[64] = {0}, ts_to_tok[64] = {0};
        uint32_t sql_limit = 0;
        int has_range = 0, has_limit = 0;
        enum { AGG_NONE, AGG_MAX, AGG_MIN, AGG_AVG, AGG_COUNT } agg = AGG_NONE;
        char agg_token[16] = {0};

        if (strcmp(line, "select min") == 0) {
            agg = AGG_MIN;
        } else if (strcmp(line, "select max") == 0) {
            agg = AGG_MAX;
        } else if (strcmp(line, "select avg") == 0) {
            agg = AGG_AVG;
        } else if (strcmp(line, "select count") == 0) {
            agg = AGG_COUNT;
        } else if (sscanf(line, "select ts(%63[^,],%63[^)]) limit %lu %15s", ts_from_tok, ts_to_tok, &sql_limit, agg_token) == 4) {
            if (!parse_timestamp_token(ts_from_tok, &ts_from_sql) || !parse_timestamp_token(ts_to_tok, &ts_to_sql)) {
                uart_send_str(uart, "ERR bad_args\n");
                return LEAFTS_ERR_NULL;
            }
            has_range = 1; has_limit = 1;
        } else if (sscanf(line, "select ts(%63[^,],%63[^)]) limit %lu", ts_from_tok, ts_to_tok, &sql_limit) == 3) {
            if (!parse_timestamp_token(ts_from_tok, &ts_from_sql) || !parse_timestamp_token(ts_to_tok, &ts_to_sql)) {
                uart_send_str(uart, "ERR bad_args\n");
                return LEAFTS_ERR_NULL;
            }
            has_range = 1; has_limit = 1;
        } else if (sscanf(line, "select ts(%63[^,],%63[^)]) %15s", ts_from_tok, ts_to_tok, agg_token) == 3) {
            if (!parse_timestamp_token(ts_from_tok, &ts_from_sql) || !parse_timestamp_token(ts_to_tok, &ts_to_sql)) {
                uart_send_str(uart, "ERR bad_args\n");
                return LEAFTS_ERR_NULL;
            }
            has_range = 1;
        } else if (sscanf(line, "select ts(%63[^,],%63[^)])", ts_from_tok, ts_to_tok) == 2) {
            if (!parse_timestamp_token(ts_from_tok, &ts_from_sql) || !parse_timestamp_token(ts_to_tok, &ts_to_sql)) {
                uart_send_str(uart, "ERR bad_args\n");
                return LEAFTS_ERR_NULL;
            }
            has_range = 1;
        } else if (sscanf(line, "select * where timestamp between %lu %lu limit %lu %15s", &ts_from_sql, &ts_to_sql, &sql_limit, agg_token) == 4) {
            // legacy alias
            has_range = 1; has_limit = 1;
        } else if (sscanf(line, "select * where timestamp between %lu %lu limit %lu", &ts_from_sql, &ts_to_sql, &sql_limit) == 3) {
            has_range = 1; has_limit = 1;
        } else if (sscanf(line, "select * where timestamp between %lu %lu %15s", &ts_from_sql, &ts_to_sql, agg_token) == 3) {
            has_range = 1;
        } else if (sscanf(line, "select * where timestamp between %lu %lu", &ts_from_sql, &ts_to_sql) == 2) {
            has_range = 1;
        } else if (sscanf(line, "select limit %lu %15s", &sql_limit, agg_token) == 2) {
            has_limit = 1;
        } else if (sscanf(line, "select limit %lu", &sql_limit) == 1) {
            has_limit = 1;
        } else if (sscanf(line, "select * limit %lu %15s", &sql_limit, agg_token) == 2) {
            has_limit = 1;
        } else if (sscanf(line, "select * limit %lu", &sql_limit) == 1) {
            has_limit = 1;
        } else if (sscanf(line, "select * %15s", agg_token) == 1) {
            // aggregate only
        }

        if (agg_token[0]) {
            if (strcmp(agg_token, "max") == 0) agg = AGG_MAX;
            else if (strcmp(agg_token, "min") == 0) agg = AGG_MIN;
            else if (strcmp(agg_token, "avg") == 0) agg = AGG_AVG;
            else if (strcmp(agg_token, "count") == 0) agg = AGG_COUNT;
            else { uart_send_str(uart, "ERR bad_args\n"); return LEAFTS_ERR_NULL; }
        }

        uint32_t *selected = NULL;
        uint32_t selected_count = 0;
        if (db->record_count > 0) {
            selected = (uint32_t *)malloc((size_t)db->record_count * sizeof(uint32_t));
            if (!selected) { uart_send_str(uart, "ERR -1\n"); return LEAFTS_ERR_NULL; }
        }

        for (uint32_t record_index = 0; record_index < db->record_count; record_index++) {
            leafts_record_t record;
            if (leafts_get_by_index(db, record_index, &record) != LEAFTS_OK) continue;
            if (has_range && (record.timestamp < ts_from_sql || record.timestamp > ts_to_sql)) continue;
            selected[selected_count++] = record_index;
        }

        if (has_limit && sql_limit < selected_count) {
            uint32_t keep = sql_limit;
            memmove(selected, selected + (selected_count - keep), (size_t)keep * sizeof(uint32_t));
            selected_count = keep;
        }

        if (agg == AGG_COUNT) {
            snprintf(response, sizeof(response), "OK count=%lu\n", (unsigned long)selected_count);
            uart_send_str(uart, response);
            free(selected);
            return LEAFTS_OK;
        }

        if (agg == AGG_AVG) {
            if (selected_count == 0) {
                free(selected);
                uart_send_str(uart, "ERR empty\n");
                return LEAFTS_ERR_EMPTY;
            }
            float sum = 0.0f;
            for (uint32_t i = 0; i < selected_count; i++) {
                leafts_record_t record;
                if (leafts_get_by_index(db, selected[i], &record) == LEAFTS_OK) sum += record.value;
            }
            snprintf(response, sizeof(response), "OK %f\n", sum / (float)selected_count);
            uart_send_str(uart, response);
            free(selected);
            return LEAFTS_OK;
        }

        if (agg == AGG_MAX || agg == AGG_MIN) {
            if (selected_count == 0) {
                free(selected);
                uart_send_str(uart, "ERR empty\n");
                return LEAFTS_ERR_EMPTY;
            }

            leafts_record_t best_record;
            leafts_get_by_index(db, selected[0], &best_record);
            for (uint32_t i = 1; i < selected_count; i++) {
                leafts_record_t record;
                if (leafts_get_by_index(db, selected[i], &record) != LEAFTS_OK) continue;
                if ((agg == AGG_MAX && record.value > best_record.value) ||
                    (agg == AGG_MIN && record.value < best_record.value)) {
                    best_record = record;
                }
            }
            snprintf(response, sizeof(response), "OK %lu %f\n", (unsigned long)best_record.timestamp, best_record.value);
            uart_send_str(uart, response);
            free(selected);
            return LEAFTS_OK;
        }

        snprintf(response, sizeof(response), "OK %lu\n", (unsigned long)selected_count);
        uart_send_str(uart, response);

        for (uint32_t i = 0; i < selected_count; i++) {
            leafts_record_t record;
            if (leafts_get_by_index(db, selected[i], &record) == LEAFTS_OK) {
                snprintf(response, sizeof(response), "%lu %f\n", (unsigned long)record.timestamp, record.value);
                uart_send_str(uart, response);
            }
        }

        free(selected);
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
                if (record.magic == LEAFTS_MAGIC_TEXT) {
                    snprintf(response, sizeof(response),
                             "%lu %.4s\n", (unsigned long)record.timestamp, record.text);
                } else {
                    snprintf(response, sizeof(response),
                             "%lu %f\n", (unsigned long)record.timestamp, record.value);
                }
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
    if (strncmp(line, "get_min", 7) == 0 || strcmp(line, "select min(value)") == 0 || strcmp(line, "select min") == 0)
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
    if (strncmp(line, "get_max", 7) == 0 || strcmp(line, "select max(value)") == 0 || strcmp(line, "select max") == 0)
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
    uint32_t del_last_n = 0;
    if (sscanf(line, "delete * limit %lu max", &del_last_n) == 1) {
        uint32_t removed = 0;
        int result = erase_extreme_from_last_n(db, del_last_n, 1, &removed);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK removed=%lu\n", (unsigned long)removed);
            uart_send_str(uart, response);
        } else if (result == LEAFTS_ERR_EMPTY) {
            uart_send_str(uart, "ERR empty\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }
    if (sscanf(line, "delete * limit %lu min", &del_last_n) == 1) {
        uint32_t removed = 0;
        int result = erase_extreme_from_last_n(db, del_last_n, 0, &removed);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK removed=%lu\n", (unsigned long)removed);
            uart_send_str(uart, response);
        } else if (result == LEAFTS_ERR_EMPTY) {
            uart_send_str(uart, "ERR empty\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    float delete_value_target = 0.0f;
    if (sscanf(line, "delete value(%f)", &delete_value_target) == 1 ||
        sscanf(line, "delete * value(%f)", &delete_value_target) == 1 ||
        sscanf(line, "delete value %f", &delete_value_target) == 1 ||
        sscanf(line, "delete value = %f", &delete_value_target) == 1 ||
        sscanf(line, "delete * value %f", &delete_value_target) == 1 ||
        sscanf(line, "delete * value = %f", &delete_value_target) == 1) {
        uint32_t removed = 0;
        int result = erase_records_by_exact_value(db, delete_value_target, &removed);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK removed=%lu\n", (unsigned long)removed);
            uart_send_str(uart, response);
        } else if (result == LEAFTS_ERR_EMPTY) {
            uart_send_str(uart, "ERR empty\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    char range_from_tok[64] = {0}, range_to_tok[64] = {0};
    uint32_t range_from = 0, range_to = 0;
    if (sscanf(line, "delete range(%63[^,],%63[^)])", range_from_tok, range_to_tok) == 2 ||
        sscanf(line, "delete * range(%63[^,],%63[^)])", range_from_tok, range_to_tok) == 2 ||
        sscanf(line, "delete ts(%63[^,],%63[^)])", range_from_tok, range_to_tok) == 2 ||
        sscanf(line, "delete * ts(%63[^,],%63[^)])", range_from_tok, range_to_tok) == 2) {
        if (!parse_timestamp_token(range_from_tok, &range_from) || !parse_timestamp_token(range_to_tok, &range_to)) {
            uart_send_str(uart, "ERR bad_args\n");
            return LEAFTS_ERR_NULL;
        }
        uint32_t removed = 0;
        int result = erase_records_in_timestamp_range(db, range_from, range_to, &removed);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK removed=%lu\n", (unsigned long)removed);
            uart_send_str(uart, response);
        } else if (result == LEAFTS_ERR_EMPTY) {
            uart_send_str(uart, "ERR empty\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    uint32_t erase_min_n = 0;
    if (sscanf(line, "delete min %lu", &erase_min_n) == 1 ||
        sscanf(line, "delete min(%lu)", &erase_min_n) == 1 ||
        sscanf(line, "delete from leafts order by value asc limit %lu", &erase_min_n) == 1 ||
        sscanf(line, "delete min(value) limit %lu", &erase_min_n) == 1 ||
        strcmp(line, "delete min") == 0 ||
        strcmp(line, "delete * min") == 0 ||
        strcmp(line, "delete min(value)") == 0)
    {
        if (strcmp(line, "delete min(value)") == 0 || strcmp(line, "delete min") == 0 || strcmp(line, "delete * min") == 0) erase_min_n = 1;
        uint32_t removed = 0;
        int result = erase_n_smallest_records(db, erase_min_n, &removed);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK removed=%lu\n", (unsigned long)removed);
            uart_send_str(uart, response);
        } else if (result == LEAFTS_ERR_EMPTY) {
            uart_send_str(uart, "ERR empty\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    uint32_t erase_max_n = 0;
    if (sscanf(line, "delete max %lu", &erase_max_n) == 1 ||
        sscanf(line, "delete max(%lu)", &erase_max_n) == 1 ||
        sscanf(line, "delete from leafts order by value desc limit %lu", &erase_max_n) == 1 ||
        sscanf(line, "delete max(value) limit %lu", &erase_max_n) == 1 ||
        strcmp(line, "delete max") == 0 ||
        strcmp(line, "delete * max") == 0 ||
        strcmp(line, "delete max(value)") == 0)
    {
        if (strcmp(line, "delete max(value)") == 0 || strcmp(line, "delete max") == 0 || strcmp(line, "delete * max") == 0) erase_max_n = 1;
        uint32_t removed = 0;
        int result = erase_n_largest_records(db, erase_max_n, &removed);
        if (result == LEAFTS_OK) {
            snprintf(response, sizeof(response), "OK removed=%lu\n", (unsigned long)removed);
            uart_send_str(uart, response);
        } else if (result == LEAFTS_ERR_EMPTY) {
            uart_send_str(uart, "ERR empty\n");
        } else {
            snprintf(response, sizeof(response), "ERR %d\n", result);
            uart_send_str(uart, response);
        }
        return result;
    }

    if (strncmp(line, "erase", 5) == 0 ||
        strcmp(line, "delete *") == 0 ||
        strcmp(line, "delete all") == 0 ||
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

    //  CLEAR (no-op on server side, useful for client parity)
    if (strcmp(line, "clear") == 0)
    {
        uart_send_str(uart, "OK\n");
        return LEAFTS_OK;
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

    if (strncmp(line, "get_avg", 7) == 0 || strcmp(line, "select avg(value)") == 0 || strcmp(line, "select avg") == 0)
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
    if (strncmp(line, "count", 5) == 0 || strcmp(line, "select count(*)") == 0 || strcmp(line, "select count") == 0)
    {
        snprintf(response, sizeof(response), "OK count=%lu\n", (unsigned long)db->record_count);
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

    //  HELP
    if (strncmp(line, "help", 4) == 0)
    {
        uart_send_str(uart, "OK 19\n");
        uart_send_str(uart, "insert <value> [timestamp|date] - add record\n");
        uart_send_str(uart, "  date format: dd.mm.yyyy[.hh[.mm[.ss]]]\n");
        uart_send_str(uart, "select - latest record\n");
        uart_send_str(uart, "select min|max|avg|count - aggregate over all records\n");
        uart_send_str(uart, "select limit <n> - last N records\n");
        uart_send_str(uart, "select limit <n> max|min|avg|count - aggregate over last N\n");
        uart_send_str(uart, "select ts(<from>,<to>) - records in timestamp/date range\n");
        uart_send_str(uart, "select ts(<from>,<to>) limit <n> [max|min|avg|count] - stacked query\n");
        uart_send_str(uart, "delete * | delete all - remove all records\n");
        uart_send_str(uart, "delete min [n] / delete min(n) - remove smallest record(s)\n");
        uart_send_str(uart, "delete max [n] / delete max(n) - remove largest record(s)\n");
        uart_send_str(uart, "delete value(<v>) - remove records with exact value\n");
        uart_send_str(uart, "delete ts(<from>,<to>) - remove records in timestamp/date range\n");
        uart_send_str(uart, "delete * limit <n> max|min - remove extreme from last N\n");
        uart_send_str(uart, "status - database info\n");
        uart_send_str(uart, "erase - remove all records (hard clear)\n");
        uart_send_str(uart, "clear - clear terminal (GUI/client side)\n");
        uart_send_str(uart, "help - show this list\n");
        uart_send_str(uart, "SQL aliases still work: select * ..., where timestamp between ..., delete from leafts ...\n");
        return LEAFTS_OK;
    }

    //  UNKNOWN COMMAND 
    uart_send_str(uart, "ERR unknown_cmd\n");
    return LEAFTS_ERR_NULL;
}
