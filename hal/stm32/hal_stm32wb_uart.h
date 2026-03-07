#ifndef HAL_STM32WB_UART_H
#define HAL_STM32WB_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32WB (PA9 = TX, PA10 = RX, AF7, 115200 8N1).
// Assumes 64 MHz PCLK2 (HSE 32 MHz × 2 via PLL, APB2 no prescaler).
// Uses USART v2 register layout (ISR/RDR/TDR) — same as STM32L4.
// GPIO and RCC are in CPU1 domain (different base from STM32L4!).
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32wb_uart_init(hal_uart_t *uart);

#endif // HAL_STM32WB_UART_H
