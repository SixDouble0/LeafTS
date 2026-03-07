// ESP32 UART0 HAL for LeafTS
// Reference: ESP32 Technical Reference Manual §12 (UART Controller)
//
// Uses direct UART0 register access.  Base address: 0x3FF40000.
// This compiles on any host toolchain (no ESP-IDF headers required):
// all register accesses are volatile pointer dereferences to fixed addresses.
// 115200 8N1, UART_CLK_DIV = APB_CLK (80 MHz) / 115200 ≈ 694.

#include "../../hal/esp32/hal_esp32_uart.h"
#include <stdint.h>

#define UART0_BASE  0x3FF40000UL

// ESP32 UART register offsets (TRM §12.4)
#define UART_FIFO   (*(volatile uint32_t *)(UART0_BASE + 0x00))  // bits[7:0] RX data / TX data
#define UART_STATUS (*(volatile uint32_t *)(UART0_BASE + 0x1C))  // bits[7:0]=RXFIFO_CNT, bits[19:16]=TXFIFO_CNT
#define UART_CLKDIV (*(volatile uint32_t *)(UART0_BASE + 0x14))  // baud clock divisor

// STATUS field masks
#define RXFIFO_CNT_MASK  0x000000FFUL   // bytes available in RX FIFO
#define TXFIFO_CNT_MASK  0x000F0000UL   // bytes in TX FIFO (0 = empty)
#define TXFIFO_CNT_SHIFT 16

// TXFIFO depth: 128 bytes; we wait until TX FIFO has room
#define TXFIFO_MAX  128u

#define TICKS_PER_MS  80000u   // ~80 MHz APB

static int esp_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while ((((UART_STATUS & TXFIFO_CNT_MASK) >> TXFIFO_CNT_SHIFT) >= TXFIFO_MAX) && t--) {}
        if (((UART_STATUS & TXFIFO_CNT_MASK) >> TXFIFO_CNT_SHIFT) >= TXFIFO_MAX) return HAL_UART_ERR;
        UART_FIFO = data[i];
    }
    return HAL_UART_OK;
}

static int esp_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(UART_STATUS & RXFIFO_CNT_MASK)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(UART_FIFO & 0xFFU);
    }
    return HAL_UART_OK;
}

int esp32_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    // UART0 is typically already configured by the ESP-IDF bootloader.
    // Set baud rate divisor for 115200 at 80 MHz APB clock.
    UART_CLKDIV = 694;   // 80 000 000 / 115 200 ≈ 694
    uart->send    = esp_send;
    uart->receive = esp_recv;
    return 0;
}
