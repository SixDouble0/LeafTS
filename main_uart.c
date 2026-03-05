#include <stdio.h>
#include <stdint.h>

#include "hal/hal_vflash.h"
#include "hal/hal_vuart.h"
#include "include/leafts.h"
#include "include/uart_handler.h"

// FLASH REGION RESERVED FOR LeafTS DATABASE
#define DB_BASE_ADDR 0x00000000U
#define DB_SIZE      (4U * 1024U)

// VIRTUAL UART PORT - PYTHON CLIENT WILL CONNECT HERE
#define VUART_PORT   5555

int main(void)
{
    hal_flash_t flash;
    hal_uart_t  uart;
    leafts_db_t db;

    // INITIALIZE VIRTUAL FLASH - 64KB RAM ARRAY SIMULATING FLASH
    vflash_init(&flash);

    // INITIALIZE DATABASE - SCAN FLASH AND RESTORE RECORD COUNT
    leafts_init(&db, &flash, DB_BASE_ADDR, DB_SIZE);

    printf("[leafts] DB initialized. Records in flash: %lu / %lu\n",
           (unsigned long)db.record_count,
           (unsigned long)db.capacity);

    // INITIALIZE VIRTUAL UART - BLOCKS UNTIL PYTHON CLIENT CONNECTS
    vuart_init(&uart, VUART_PORT);

    // MAIN LOOP - READ ONE LINE AT A TIME AND PROCESS AS COMMAND
    printf("[leafts] Ready. Waiting for commands...\n");

    char    line[128];
    uint8_t ch;
    uint32_t line_len;

    while (1)
    {
        // ACCUMULATE BYTES UNTIL NEWLINE OR BUFFER FULL
        line_len = 0;

        while (line_len < sizeof(line) - 1)
        {
            if (uart.receive(&ch, 1, 10000) != HAL_UART_OK)
            {
                // TIMEOUT - NO DATA FROM CLIENT, CONTINUE WAITING
                break;
            }

            if (ch == '\n' || ch == '\r')
            {
                break;
            }

            line[line_len++] = (char)ch;
        }

        // SKIP EMPTY LINES
        if (line_len == 0) continue;

        line[line_len] = '\0';

        printf("[leafts] CMD: %s\n", line);

        // DISPATCH COMMAND TO uart_handler - IT CALLS leafts_* AND SENDS RESPONSE
        uart_handler_process(line, &db, &uart);
    }

    return 0;
}
