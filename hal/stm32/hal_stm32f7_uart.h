#ifndef HAL_STM32F7_UART_H
#define HAL_STM32F7_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32F7 (PA9 = TX, PA10 = RX, 115200 8N1).
// Assumes 108 MHz APB2 clock (standard PLL: 216 MHz SYSCLK, APB2 prescaler /2).
// Uses USART v2 register layout (ISR/RDR/TDR) — unlike STM32F4 which uses v1 (SR/DR).
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32f7_uart_init(hal_uart_t *uart);

#endif // HAL_STM32F7_UART_H
