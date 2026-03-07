#ifndef HAL_NRF52_UART_H
#define HAL_NRF52_UART_H

#include "hal_uart.h"

// Initialize UART0 on nRF52 (P0.06 = TX, P0.08 = RX, 115200 8N1).
// Pin numbers match the default UART pins on the nRF52840-DK and nRF52-DK.
// Uses the legacy UART0 peripheral (not UARTE/EasyDMA) for simplicity.
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int nrf52_uart_init(hal_uart_t *uart);

#endif // HAL_NRF52_UART_H
