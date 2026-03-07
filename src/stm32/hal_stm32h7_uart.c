// STM32H7 USART1 HAL for LeafTS
// Reference: RM0433 (STM32H742/H743/H750/H753)
//
// USART v2 register layout.  USART1 @ 0x40011000, APB2.
// GPIOA @ 0x58020000 (AHB4 bus — different from all other STM32 families).
// RCC @ 0x58024400.
// Typical APB2 clock: 120 MHz (SYSCLK 480 MHz, D2PPRE2 prescaler /4).
// BRR = 120 000 000 / 115 200 ≈ 1042.

#include "../../hal/stm32/hal_stm32h7_uart.h"
#include <stdint.h>

#define USART1_BASE  0x40011000UL
#define GPIOA_BASE   0x58020000UL   // AHB4 bus (D3 domain)
#define RCC_BASE     0x58024400UL

#define USART_CR1  (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART_BRR  (*(volatile uint32_t *)(USART1_BASE + 0x0C))
#define USART_ISR  (*(volatile uint32_t *)(USART1_BASE + 0x1C))
#define USART_RDR  (*(volatile uint32_t *)(USART1_BASE + 0x24))
#define USART_TDR  (*(volatile uint32_t *)(USART1_BASE + 0x28))

#define ISR_RXNE  (1UL << 5)
#define ISR_TXE   (1UL << 7)
#define CR1_UE    (1UL << 0)
#define CR1_RE    (1UL << 2)
#define CR1_TE    (1UL << 3)

// RCC: AHB4ENR (GPIOA = bit 0), APB2ENR (USART1 = bit 4)
#define RCC_AHB4ENR  (*(volatile uint32_t *)(RCC_BASE + 0xE0))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0xF0))

#define GPIOA_MODER  (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH   (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define TICKS_PER_MS  120000u

static int h7_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while (!(USART_ISR & ISR_TXE) && t--) {}
        if (!(USART_ISR & ISR_TXE)) return HAL_UART_ERR;
        USART_TDR = data[i];
    }
    return HAL_UART_OK;
}
static int h7_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(USART_ISR & ISR_RXNE)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(USART_RDR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32h7_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    RCC_AHB4ENR |= (1UL << 0);   // GPIOAEN (AHB4, D3 domain)
    RCC_APB2ENR |= (1UL << 4);   // USART1EN
    // PA9 = TX, PA10 = RX → AF mode (MODER = 0b10)
    GPIOA_MODER = (GPIOA_MODER & ~(0x3UL << 18 | 0x3UL << 20))
                | (0x2UL << 18) | (0x2UL << 20);
    // AFRH: AF7 for PA9 (bits [7:4]) and PA10 (bits [11:8])
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFFUL << 4))
               | (7UL << 4) | (7UL << 8);
    USART_CR1 = 0;
    USART_BRR = 1042;  // 120 000 000 / 115 200
    USART_CR1 = CR1_TE | CR1_RE | CR1_UE;
    uart->send    = h7_send;
    uart->receive = h7_recv;
    return 0;
}
