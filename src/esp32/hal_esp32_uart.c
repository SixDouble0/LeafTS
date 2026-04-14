// ESP32 UART0 HAL for LeafTS
//
// When compiled under ESP-IDF (ESP_PLATFORM defined) uses the esp_idf UART
// driver so the FreeRTOS scheduler is not starved.
// When compiled bare-metal (no ESP_PLATFORM) falls back to direct register
// access — suitable for bootloader or no-OS builds.

#include "../../hal/esp32/hal_esp32_uart.h"
#include <stdint.h>

#ifdef ESP_PLATFORM

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LEAFTS_UART_NUM  UART_NUM_0
#define LEAFTS_UART_BAUD 115200
#define LEAFTS_RX_BUF    256

static int esp_send(const uint8_t *data, uint32_t len)
{
    int written = uart_write_bytes(LEAFTS_UART_NUM, (const char *)data, (size_t)len);
    return (written == (int)len) ? HAL_UART_OK : HAL_UART_ERR;
}

static int esp_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    for (uint32_t i = 0; i < len; i++) {
        int r = uart_read_bytes(LEAFTS_UART_NUM, &buf[i], 1,
                                pdMS_TO_TICKS(timeout_ms));
        if (r <= 0) return HAL_UART_TIMEOUT;
    }
    return HAL_UART_OK;
}

int esp32_uart_init(hal_uart_t *uart)
{
    if (!uart) return -1;
    uart_config_t cfg = {
        .baud_rate  = LEAFTS_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(LEAFTS_UART_NUM, LEAFTS_RX_BUF, 0, 0, NULL, 0);
    uart_param_config(LEAFTS_UART_NUM, &cfg);
    uart->send    = esp_send;
    uart->receive = esp_recv;
    return 0;
}

#else // bare-metal fallback

#define UART0_BASE  0x3FF40000UL
#define UART_FIFO   (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_STATUS (*(volatile uint32_t *)(UART0_BASE + 0x1C))
#define UART_CLKDIV (*(volatile uint32_t *)(UART0_BASE + 0x14))
#define RXFIFO_CNT_MASK  0x000000FFUL
#define TXFIFO_CNT_MASK  0x000F0000UL
#define TXFIFO_CNT_SHIFT 16
#define TXFIFO_MAX  128u
#define TICKS_PER_MS  80000u

static int esp_send(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while ((((UART_STATUS & TXFIFO_CNT_MASK) >> TXFIFO_CNT_SHIFT) >= TXFIFO_MAX) && t--) {}
        if (((UART_STATUS & TXFIFO_CNT_MASK) >> TXFIFO_CNT_SHIFT) >= TXFIFO_MAX) return HAL_UART_ERR;
        UART_FIFO = data[i];
    }
    return HAL_UART_OK;
}

static int esp_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(UART_STATUS & RXFIFO_CNT_MASK)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(UART_FIFO & 0xFFU);
    }
    return HAL_UART_OK;
}

int esp32_uart_init(hal_uart_t *uart)
{
    if (!uart) return -1;
    UART_CLKDIV = 694;
    uart->send    = esp_send;
    uart->receive = esp_recv;
    return 0;
}

#endif // ESP_PLATFORM
