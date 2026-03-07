#ifndef HAL_STM32L4_UART_H
#define HAL_STM32L4_UART_H

#include "hal_uart.h"

// Initialize USART2 on STM32L4 (PA2=TX, PA3=RX, 115200 8N1).
// Populates the hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int stm32l4_uart_init(hal_uart_t *uart);

#endif // HAL_STM32L4_UART_H
