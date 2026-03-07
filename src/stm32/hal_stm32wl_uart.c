// STM32WL UART HAL for LeafTS
// Reference: RM0453 (STM32WL55/WL54/WLE5/WLE4)
//
// USART v2 register layout (CR1/BRR/ISR/RDR/TDR).
// USART1 @ 0x40013800, APB2.  GPIO: GPIOA (bit 0 of RCC_AHB2ENR).
// Typical APB2 clock: 48 MHz.  BRR = 48 000 000 / 115 200 = 417.

#include "../../hal/stm32/hal_stm32wl_uart.h"

#define USART1_BASE  0x40013800UL
#define GPIOA_BASE   0x48000000UL
#define RCC_BASE     0x58000000UL

// USART v2 registers
#define USART_CR1  (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART_BRR  (*(volatile uint32_t *)(USART1_BASE + 0x0C))
#define USART_ISR  (*(volatile uint32_t *)(USART1_BASE + 0x1C))
#define USART_ICR  (*(volatile uint32_t *)(USART1_BASE + 0x20))
#define USART_RDR  (*(volatile uint32_t *)(USART1_BASE + 0x24))
#define USART_TDR  (*(volatile uint32_t *)(USART1_BASE + 0x28))

#define ISR_RXNE  (1UL << 5)
#define ISR_TXE   (1UL << 7)
#define ISR_TC    (1UL << 6)
#define CR1_UE    (1UL << 0)
#define CR1_RE    (1UL << 2)
#define CR1_TE    (1UL << 3)

// GPIO A registers (AHB2 bus)
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH    (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

// RCC: AHB2ENR (GPIOAEN = bit 0), APB2ENR (USART1EN = bit 14)
#define RCC_AHB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x4C))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x60))

#define TICKS_PER_MS  48000u

static int wl_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while (!(USART_ISR & ISR_TXE) && t--) {}
        if (!(USART_ISR & ISR_TXE)) return HAL_UART_ERR;
        USART_TDR = data[i];
    }
    return HAL_UART_OK;
}
static int wl_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(USART_ISR & ISR_RXNE)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(USART_RDR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32wl_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    // Enable GPIOA (AHB2) and USART1 (APB2) clocks
    RCC_AHB2ENR |= (1UL << 0);
    RCC_APB2ENR |= (1UL << 14);
    // PA9 = TX (AF7), PA10 = RX (AF7)
    // MODER: pins 9,10 → AF mode (0b10)
    GPIOA_MODER = (GPIOA_MODER & ~(0x3UL << 18 | 0x3UL << 20))
                | (0x2UL << 18) | (0x2UL << 20);
    // AFRH: AF7 for PA9 (bits [7:4]) and PA10 (bits [11:8])
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFFUL << 4))
               | (7UL << 4) | (7UL << 8);
    // Configure USART1: 115200 8N1 at 48 MHz
    USART_CR1 = 0;
    USART_BRR = 417;  // 48000000 / 115200
    USART_CR1 = CR1_TE | CR1_RE | CR1_UE;
    uart->send    = wl_send;
    uart->receive = wl_recv;
    return 0;
}
