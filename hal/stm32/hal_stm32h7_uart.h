#ifndef HAL_STM32H7_UART_H
#define HAL_STM32H7_UART_H

#include "hal_uart.h"

// Initialize USART1 on STM32H7 (PA9 = TX, PA10 = RX, AF7, 115200 8N1).
// Assumes 100 MHz APB2 clock (standard: 400 MHz SYSCLK, APB2 prescaler /4).
// Uses USART v2 register layout (ISR/RDR/TDR).
// Note: STM32H7 RCC and GPIO are in the D2/AHB4 domain — registers differ from L4!
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32h7_uart_init(hal_uart_t *uart);

#endif // HAL_STM32H7_UART_H
