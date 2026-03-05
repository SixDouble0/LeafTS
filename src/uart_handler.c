#include <stdio.h>
#include <string.h>

#include "../include/uart_handler.h"

// HELPER: SEND A NULL-TERMINATED STRING OVER UART
static void uart_send_str(hal_uart_t *uart, const char *str)
{
    uart->send((const uint8_t *)str, (uint32_t)strlen(str));
}

// ------------------------------------------------------------------ //

// PROCESS ONE COMPLETE LINE - DISPATCH TO CORRECT leafts_* FUNCTION
// PROTOCOL:
//   append <timestamp> <value>  ->  "OK\n"  or  "ERR <code>\n"
//   latest                      ->  "OK <timestamp> <value>\n"
//   list                        ->  "OK <count>\n" + one line per record
//   status                      ->  "OK count=N capacity=N\n"
//   erase                       ->  "OK\n"
int uart_handler_process(const char *line, leafts_db_t *db, hal_uart_t *uart)
{
    char response[128];

    // ---- APPEND ----
    if (strncmp(line, "append", 6) == 0)
    {
        uint32_t timestamp;
        float    value;

        // PARSE TWO ARGUMENTS FROM LINE
        if (sscanf(line, "append %lu %f", &timestamp, &value) != 2)
        {
            uart_send_str(uart, "ERR bad_args\n");
            return LEAFTS_ERR_NULL;
        }

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

    // ---- LATEST ----
    if (strncmp(line, "latest", 6) == 0)
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

    // ---- LIST ----
    if (strncmp(line, "list", 4) == 0)
    {
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

    // ---- STATUS ----
    if (strncmp(line, "status", 6) == 0)
    {
        snprintf(response, sizeof(response),
                 "OK count=%lu capacity=%lu\n",
                 (unsigned long)db->record_count,
                 (unsigned long)db->capacity);
        uart_send_str(uart, response);
        return LEAFTS_OK;
    }

    // ---- ERASE ----
    if (strncmp(line, "erase", 5) == 0)
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

    // ---- UNKNOWN COMMAND ----
    uart_send_str(uart, "ERR unknown_cmd\n");
    return LEAFTS_ERR_NULL;
}
