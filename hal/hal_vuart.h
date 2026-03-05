#ifndef HAL_VUART_H
#define HAL_VUART_H

#include "hal_uart.h"
#include <stdint.h>

// INITIALIZE VIRTUAL UART - TCP SERVER ON GIVEN PORT
// BLOCKS UNTIL PYTHON CLIENT CONNECTS
void vuart_init(hal_uart_t *hal, uint16_t port);

#endif // HAL_VUART_H
