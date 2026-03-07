#ifndef HAL_STM32L1_UART_H
#define HAL_STM32L1_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32L1 (PA9 = TX, PA10 = RX, 115200 8N1).
// Assumes 32 MHz SYSCLK (HSI 16 MHz × 2 via PLL, APB2 no prescaler).
// Uses USART v1 register layout (SR + DR) — same as STM32F1.
// Note: STM32L1 GPIO uses MODER/AFRL (like L4), NOT CRH (unlike F1).
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32l1_uart_init(hal_uart_t *uart);

#endif // HAL_STM32L1_UART_H
