// LeafTS demo for Renode - STM32L476 emulation.
//
// This is "user code" - it wires the LeafTS library to real STM32L4 hardware:
//   - UART  : hal_stm32l4_uart.c  (USART2 registers - Renode emulates these)
//   - Flash : hal_vflash.c        (RAM array - Renode has no L4 flash controller)
//
// On real STM32L4 hardware, replace the two vflash lines with:
//   stm32l4_flash_init(&flash, 0x08040000, 128 * 1024);
//   leafts_init(&db, &flash, 0x08040000, 128 * 1024);
//
// Commands (type in the Renode UART Analyzer window):
//   insert <value>                e.g. insert 23.5
//   insert <value> <timestamp>    e.g. insert 23.5 1700000000  (alias: append)
//   select                        (alias: latest)
//   select *                      (alias: list)
//   get_last <n>
//   get_range <ts_from> <ts_to>
//   get_min
//   get_max
//   status
//   erase

#include <string.h>
#include <stdint.h>

#include "hal/hal_vflash.h"
#include "hal/stm32/hal_stm32l4_uart.h"
#include "include/leafts.h"
#include "include/uart_handler.h"

#define DB_BASE_ADDR  0x00000000U
#define DB_SIZE       (4U * 1024U)

int main(void)
{
    hal_flash_t flash;
    hal_uart_t  uart;
    leafts_db_t db;

    // Flash: software simulation (swap for stm32l4_flash_init on real HW)
    vflash_init(&flash);
    leafts_init(&db, &flash, DB_BASE_ADDR, DB_SIZE);

    // UART: real STM32L4 USART2 registers (PA2=TX, PA3=RX, 115200 8N1)
    stm32l4_uart_init(&uart);

    // Announce over UART (visible in Renode UART Analyzer)
    const char *banner = "\r\nLeafTS ready. Type a command:\r\n> ";
    uart.send((const uint8_t *)banner, (uint32_t)strlen(banner));

    char    line[128];
    uint8_t ch;
    uint32_t len = 0;

    while (1)
    {
        // Read one byte at a time, echo it back, build line until Enter
        if (uart.receive(&ch, 1, 100) != HAL_UART_OK)
            continue;

        // Echo character back so user sees what they typed
        uart.send(&ch, 1);

        if (ch == '\r' || ch == '\n') {
            if (len == 0) {
                uart.send((const uint8_t *)"> ", 2);
                continue;
            }
            line[len] = '\0';
            uart.send((const uint8_t *)"\r\n", 2);

            uart_handler_process(line, &db, &uart);

            uart.send((const uint8_t *)"> ", 2);
            len = 0;
        } else if (ch == 0x7F || ch == '\b') {
            // Backspace
            if (len > 0) {
                len--;
                uart.send((const uint8_t *)"\b \b", 3);
            }
        } else if (len < sizeof(line) - 1) {
            line[len++] = (char)ch;
        }
    }
}
