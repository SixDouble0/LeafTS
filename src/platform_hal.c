#include "../include/platform_hal.h"

#ifdef LEAFTS_PLATFORM_VIRTUAL
#include "../hal/hal_vflash.h"
#include "../hal/hal_vuart.h"
#elif defined(LEAFTS_PLATFORM_STM32F1)
#include "../hal/stm32/hal_stm32f1_flash.h"
#include "../hal/stm32/hal_stm32f1_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32F2)
#include "../hal/stm32/hal_stm32f2_flash.h"
#include "../hal/stm32/hal_stm32f2_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32F3)
#include "../hal/stm32/hal_stm32f3_flash.h"
#include "../hal/stm32/hal_stm32f3_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32F4)
#include "../hal/stm32/hal_stm32f4_flash.h"
#include "../hal/stm32/hal_stm32f4_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32F7)
#include "../hal/stm32/hal_stm32f7_flash.h"
#include "../hal/stm32/hal_stm32f7_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32G0)
#include "../hal/stm32/hal_stm32g0_flash.h"
#include "../hal/stm32/hal_stm32g0_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32G4)
#include "../hal/stm32/hal_stm32g4_flash.h"
#include "../hal/stm32/hal_stm32g4_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32H7)
#include "../hal/stm32/hal_stm32h7_flash.h"
#include "../hal/stm32/hal_stm32h7_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32L1)
#include "../hal/stm32/hal_stm32l1_flash.h"
#include "../hal/stm32/hal_stm32l1_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32L4)
#include "../hal/stm32/hal_stm32l4_flash.h"
#include "../hal/stm32/hal_stm32l4_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32L5)
#include "../hal/stm32/hal_stm32l5_flash.h"
#include "../hal/stm32/hal_stm32l5_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32WB)
#include "../hal/stm32/hal_stm32wb_flash.h"
#include "../hal/stm32/hal_stm32wb_uart.h"
#elif defined(LEAFTS_PLATFORM_STM32WL)
#include "../hal/stm32/hal_stm32wl_flash.h"
#include "../hal/stm32/hal_stm32wl_uart.h"
#elif defined(LEAFTS_PLATFORM_NRF52)
#include "../hal/nrf/hal_nrf52_flash.h"
#include "../hal/nrf/hal_nrf52_uart.h"
#elif defined(LEAFTS_PLATFORM_RP2040)
#include "../hal/rp/hal_rp2040_flash.h"
#include "../hal/rp/hal_rp2040_uart.h"
#elif defined(LEAFTS_PLATFORM_ESP32)
#include "../hal/esp32/hal_esp32_flash.h"
#include "../hal/esp32/hal_esp32_uart.h"
#else
#error "Unknown LEAFTS platform macro"
#endif

int platform_flash_init(hal_flash_t *flash, uint32_t base, uint32_t size)
{
#ifdef LEAFTS_PLATFORM_VIRTUAL
    (void)base;
    (void)size;
    return vflash_init(flash);
#elif defined(LEAFTS_PLATFORM_STM32F1)
    return stm32f1_flash_init(flash, base, size, LEAFTS_STM32F1_PAGE_SIZE);
#elif defined(LEAFTS_PLATFORM_STM32F2)
    return stm32f2_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32F3)
    return stm32f3_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32F4)
    return stm32f4_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32F7)
    return stm32f7_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32G0)
    return stm32g0_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32G4)
    return stm32g4_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32H7)
    return stm32h7_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32L1)
    return stm32l1_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32L4)
    return stm32l4_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32L5)
    return stm32l5_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32WB)
    return stm32wb_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_STM32WL)
    return stm32wl_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_NRF52)
    return nrf52_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_RP2040)
    return rp2040_flash_init(flash, base, size);
#elif defined(LEAFTS_PLATFORM_ESP32)
    return esp32_flash_init(flash, base, size);
#endif
}

int platform_uart_init(hal_uart_t *uart, uint16_t vuart_port)
{
#ifdef LEAFTS_NO_UART
    (void)uart;
    (void)vuart_port;
    return -1;
#elif defined(LEAFTS_PLATFORM_VIRTUAL)
    vuart_init(uart, vuart_port);
    return 0;
#elif defined(LEAFTS_PLATFORM_STM32F1)
    (void)vuart_port;
    return stm32f1_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32F2)
    (void)vuart_port;
    return stm32f2_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32F3)
    (void)vuart_port;
    return stm32f3_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32F4)
    (void)vuart_port;
    return stm32f4_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32F7)
    (void)vuart_port;
    return stm32f7_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32G0)
    (void)vuart_port;
    return stm32g0_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32G4)
    (void)vuart_port;
    return stm32g4_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32H7)
    (void)vuart_port;
    return stm32h7_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32L1)
    (void)vuart_port;
    return stm32l1_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32L4)
    (void)vuart_port;
    return stm32l4_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32L5)
    (void)vuart_port;
    return stm32l5_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32WB)
    (void)vuart_port;
    return stm32wb_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_STM32WL)
    (void)vuart_port;
    return stm32wl_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_NRF52)
    (void)vuart_port;
    return nrf52_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_RP2040)
    (void)vuart_port;
    return rp2040_uart_init(uart);
#elif defined(LEAFTS_PLATFORM_ESP32)
    (void)vuart_port;
    return esp32_uart_init(uart);
#endif
}
