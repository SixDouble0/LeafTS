// RP2040 UART0 HAL for LeafTS
// Reference: RP2040 Datasheet §4.2 (PL011 UART), §2.19.2 (IO_BANK0)
//
// PL011 UART0 at 0x40034000.  Default pinout: GPIO0 = TX, GPIO1 = RX.
// GPIO function select: IO_BANK0 GPIO0/1 CTRL register, FUNCSEL = 2 (UART).
// Clock: 48 MHz reference clock.
// BRR: IBRD = 26, FBRD = 3  (48 000 000 / (16 × 115 200) ≈ 26.042)

#include "../../hal/rp/hal_rp2040_uart.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// PL011 UART0 register map (RP2040 §4.2.8)
// ---------------------------------------------------------------------------
#define UART0_BASE  0x40034000UL

#define UART_DR      (*(volatile uint32_t *)(UART0_BASE + 0x000))
#define UART_FR      (*(volatile uint32_t *)(UART0_BASE + 0x018))
#define UART_IBRD    (*(volatile uint32_t *)(UART0_BASE + 0x024))
#define UART_FBRD    (*(volatile uint32_t *)(UART0_BASE + 0x028))
#define UART_LCR_H   (*(volatile uint32_t *)(UART0_BASE + 0x02C))
#define UART_CR      (*(volatile uint32_t *)(UART0_BASE + 0x030))

// UART_FR bits
#define FR_BUSY  (1UL << 3)
#define FR_RXFE  (1UL << 4)
#define FR_TXFF  (1UL << 5)
// UART_LCR_H bits
#define LCR_WLEN_8  (0x3UL << 5)   // 8-bit word length
// UART_CR bits
#define CR_UARTEN  (1UL << 0)
#define CR_TXE     (1UL << 8)
#define CR_RXE     (1UL << 9)

// ---------------------------------------------------------------------------
// IO_BANK0 GPIO function select (§2.19.6.1)
// GPIO pad control: each GPIO has a CTRL register at GPIO_BASE + GPIO*8 + 4
// ---------------------------------------------------------------------------
#define IO_BANK0_BASE  0x40014000UL
#define GPIO0_CTRL     (*(volatile uint32_t *)(IO_BANK0_BASE + 0x004))
#define GPIO1_CTRL     (*(volatile uint32_t *)(IO_BANK0_BASE + 0x00C))
#define FUNCSEL_UART   2UL

// RESETS: unreset UART0 and IO_BANK0
#define RESETS_BASE       0x4000C000UL
#define RESETS_RESET      (*(volatile uint32_t *)(RESETS_BASE + 0x00))
#define RESETS_RESET_DONE (*(volatile uint32_t *)(RESETS_BASE + 0x08))
#define RESET_UART0       (1UL << 22)
#define RESET_IO_BANK0    (1UL << 5)
#define RESET_PADS_BANK0  (1UL << 8)

#define TICKS_PER_MS  48000u

static int rp_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while ((UART_FR & FR_TXFF) && t--) {}
        if (UART_FR & FR_TXFF) return HAL_UART_ERR;
        UART_DR = data[i];
    }
    return HAL_UART_OK;
}
static int rp_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (UART_FR & FR_RXFE) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(UART_DR & 0xFFU);
    }
    return HAL_UART_OK;
}

int rp2040_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    // Release UART0, IO_BANK0 and PADS_BANK0 from reset
    RESETS_RESET &= ~(RESET_UART0 | RESET_IO_BANK0 | RESET_PADS_BANK0);
    while (~RESETS_RESET_DONE & (RESET_UART0 | RESET_IO_BANK0 | RESET_PADS_BANK0)) {}
    // GPIO0 = TX (UART0 RX/TX, FUNCSEL=2), GPIO1 = RX
    GPIO0_CTRL = FUNCSEL_UART;
    GPIO1_CTRL = FUNCSEL_UART;
    // Disable UART before configuring
    UART_CR = 0;
    while (UART_FR & FR_BUSY) {}
    // Set baud rate: 48 MHz / (16 × 115200) = 26.042  → IBRD=26, FBRD=3
    UART_IBRD  = 26;
    UART_FBRD  = 3;
    UART_LCR_H = LCR_WLEN_8;   // 8N1, FIFOs disabled
    UART_CR    = CR_UARTEN | CR_TXE | CR_RXE;
    uart->send    = rp_send;
    uart->receive = rp_recv;
    return 0;
}
