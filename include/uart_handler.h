#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include "../hal/hal_uart.h"
#include "leafts.h"

// PROCESS ONE COMPLETE LINE RECEIVED FROM UART
// PARSES COMMAND, CALLS APPROPRIATE leafts_* FUNCTION, SENDS RESPONSE
// RETURNS LEAFTS_OK OR ERROR CODE
int uart_handler_process(const char *line, leafts_db_t *db, hal_uart_t *uart);

#endif // UART_HANDLER_H
