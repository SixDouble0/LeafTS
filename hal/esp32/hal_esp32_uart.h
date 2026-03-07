#ifndef HAL_ESP32_UART_H
#define HAL_ESP32_UART_H

#include "hal_uart.h"

// Initialize UART0 on ESP32/ESP32-S2/ESP32-S3 (GPIO1 = TX, GPIO3 = RX, 115200 8N1).
// GPIO1 and GPIO3 are the default UART0 pins on most ESP32 modules and devkits.
// Baud rate: 115200.
// This implementation writes directly to the UART0 FIFO registers.
// Note: no ESP-IDF driver is used — register-level bare-metal implementation.
// Populates hal_uart_t function pointers.
// Returns 0 on success, -1 on bad parameter.
int esp32_uart_init(hal_uart_t *uart);

#endif // HAL_ESP32_UART_H
