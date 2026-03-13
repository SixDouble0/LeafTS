#ifndef PLATFORM_HAL_H
#define PLATFORM_HAL_H

#include <stdint.h>
#include "../hal/hal_flash.h"
#include "../hal/hal_uart.h"

int platform_flash_init(hal_flash_t *flash, uint32_t base, uint32_t size);
int platform_uart_init(hal_uart_t *uart, uint16_t vuart_port);

#endif // PLATFORM_HAL_H
