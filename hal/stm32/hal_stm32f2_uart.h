#ifndef HAL_STM32F2_UART_H
#define HAL_STM32F2_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32F2 (PA9 = TX, PA10 = RX, 115200 8N1).
// Assumes 60 MHz APB2 clock (standard PLL: 120 MHz SYSCLK, APB2 prescaler /2).
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32f2_uart_init(hal_uart_t *uart);

#endif // HAL_STM32F2_UART_H
