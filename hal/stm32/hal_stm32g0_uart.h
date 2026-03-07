#ifndef HAL_STM32G0_UART_H
#define HAL_STM32G0_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32G0 (PA9 = TX, PA10 = RX, 115200 8N1).
// Assumes 16 MHz HSI clock (reset default) — BRR = 139.
// If PLL is active, recalculate BRR: BRR = PCLK / 115200.
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32g0_uart_init(hal_uart_t *uart);

#endif // HAL_STM32G0_UART_H
