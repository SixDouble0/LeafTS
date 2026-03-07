#ifndef HAL_RP2040_UART_H
#define HAL_RP2040_UART_H

#include "hal_uart.h"

// Initialize UART0 on RP2040/RP2350 (GPIO0 = TX, GPIO1 = RX, 115200 8N1).
// Assumes 125 MHz system clock (default PLL configuration).
// Uses ARM PrimeCell UART (PL011) register layout at UART0_BASE = 0x40034000.
// UART0 GPIO0/GPIO1 are the default pins on Raspberry Pi Pico.
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int rp2040_uart_init(hal_uart_t *uart);

#endif // HAL_RP2040_UART_H
