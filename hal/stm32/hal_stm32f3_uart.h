#ifndef HAL_STM32F3_UART_H
#define HAL_STM32F3_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32F3 (PA9 = TX, PA10 = RX, 115200 8N1).
// Assumes 72 MHz APB2 clock (standard 8 MHz HSE + PLL × 9).
// Uses the same USART v1 (SR + DR) register layout as STM32F1.
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32f3_uart_init(hal_uart_t *uart);

#endif // HAL_STM32F3_UART_H
