// nRF52 UART0 HAL implementation.
// Reference: nRF52840 Product Specification v1.7, §6.34 UART (legacy UART0)
//
// Uses the legacy UART0 peripheral (not UARTE/EasyDMA) for simplicity.
// Pinout : P0.06 = TX, P0.08 = RX  (default on nRF52840-DK and nRF52-DK)
// Baud   : 115200 (BAUDRATE register value 0x01D7E000)
//
// Note: TX/RX pin numbers are written directly to the PSELTXD/PSELRXD registers.
//       Disconnect a pin by writing 0xFFFFFFFF (bit 31 = CONNECT=1).

#include "../hal/hal_nrf52_uart.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// UART0 register map (nRF52840 PS §6.34.5)
// ---------------------------------------------------------------------------
#define UART0_BASE  0x40002000UL

#define TASKS_STARTRX  (*(volatile uint32_t *)(UART0_BASE + 0x000))
#define TASKS_STOPRX   (*(volatile uint32_t *)(UART0_BASE + 0x004))
#define TASKS_STARTTX  (*(volatile uint32_t *)(UART0_BASE + 0x008))
#define TASKS_STOPTX   (*(volatile uint32_t *)(UART0_BASE + 0x00C))
#define EVENTS_RXDRDY  (*(volatile uint32_t *)(UART0_BASE + 0x108))  // RX byte ready
#define EVENTS_TXDRDY  (*(volatile uint32_t *)(UART0_BASE + 0x11C))  // TX byte sent
#define ENABLE         (*(volatile uint32_t *)(UART0_BASE + 0x500))
#define PSELTXD        (*(volatile uint32_t *)(UART0_BASE + 0x50C))  // TX pin select
#define PSELRXD        (*(volatile uint32_t *)(UART0_BASE + 0x514))  // RX pin select
#define RXD            (*(volatile uint32_t *)(UART0_BASE + 0x518))  // received byte
#define TXD            (*(volatile uint32_t *)(UART0_BASE + 0x51C))  // byte to transmit
#define BAUDRATE       (*(volatile uint32_t *)(UART0_BASE + 0x524))
#define CONFIG         (*(volatile uint32_t *)(UART0_BASE + 0x56C))

#define UART_ENABLE_VAL   4UL   // value that enables UART0
#define UART_DISABLE_VAL  0UL

// Baud rate register value for 115200 (from nRF52 PS Table 421)
#define BAUD_115200  0x01D7E000UL

// Pin numbers for nRF52840-DK / nRF52-DK default UART pins
#define PIN_TX  6UL   // P0.06
#define PIN_RX  8UL   // P0.08

// Disconnect value (PSEL.CONNECT bit 31 = 1 means disconnected)
#define PSEL_DISCONNECT  0xFFFFFFFFUL

// ---------------------------------------------------------------------------
// Timeout
// ---------------------------------------------------------------------------
#define TICKS_PER_MS  64000UL   // ~64 000 ticks per ms at 64 MHz

// ---------------------------------------------------------------------------
// Internal send / receive
// ---------------------------------------------------------------------------
static int uart_send(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        EVENTS_TXDRDY = 0;
        TXD = data[i];
        while (!EVENTS_TXDRDY) {}  // wait until byte transmitted
    }
    return HAL_UART_OK;
}

static int uart_receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    uint32_t deadline = timeout_ms * TICKS_PER_MS;

    for (uint32_t i = 0; i < len; i++) {
        EVENTS_RXDRDY = 0;
        uint32_t ticks = 0;
        while (!EVENTS_RXDRDY) {
            if (++ticks >= deadline) return HAL_UART_TIMEOUT;
        }
        buf[i] = (uint8_t)(RXD & 0xFF);
    }
    return HAL_UART_OK;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int nrf52_uart_init(hal_uart_t *uart)
{
    if (!uart) return -1;

    // 1. Disable UART before configuring
    ENABLE = UART_DISABLE_VAL;

    // 2. Select TX and RX pins (P0.06 = TX, P0.08 = RX on nRF52840-DK)
    PSELTXD = PIN_TX;
    PSELRXD = PIN_RX;

    // 3. Set baud rate to 115200
    BAUDRATE = BAUD_115200;

    // 4. No hardware flow control (CONFIG = 0)
    CONFIG = 0;

    // 5. Enable UART0
    ENABLE = UART_ENABLE_VAL;

    // 6. Start RX and TX tasks
    TASKS_STARTRX = 1;
    TASKS_STARTTX = 1;

    uart->send    = uart_send;
    uart->receive = uart_receive;
    return 0;
}
