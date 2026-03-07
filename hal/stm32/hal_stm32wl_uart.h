#ifndef HAL_STM32WL_UART_H
#define HAL_STM32WL_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32WL (PA9 = TX, PA10 = RX, AF7, 115200 8N1).
// Assumes 48 MHz PCLK2 (HSE 48 MHz, no prescaler — or MSI 48 MHz).
// Uses USART v2 register layout (ISR/RDR/TDR) — same as STM32L4.
// RCC and GPIO are in the same CPU1 peripheral space as STM32WB.
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32wl_uart_init(hal_uart_t *uart);

#endif // HAL_STM32WL_UART_H
