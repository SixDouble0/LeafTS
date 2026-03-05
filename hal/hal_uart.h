#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

// UART HAL ERROR CODES
#define HAL_UART_OK      0
#define HAL_UART_ERR    (-1)
#define HAL_UART_TIMEOUT (-2)

// UART HAL INTERFACE - SAME PATTERN AS hal_flash_t
// SEND AND RECEIVE ARE FUNCTION POINTERS - IMPLEMENTATION IS SWAPPABLE PER PLATFORM
typedef struct
{
    int (*send)   (const uint8_t *data, uint32_t len);
    int (*receive)(uint8_t *buf,        uint32_t len, uint32_t timeout_ms);
} hal_uart_t;

#endif // HAL_UART_H
