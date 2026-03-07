// STM32L5 USART1 HAL for LeafTS
// Reference: RM0438 (STM32L552/L562)
//
// USART v2 register layout (same as STM32L4).
// USART1 @ 0x40013800, APB2.  PA9 = TX, PA10 = RX, AF7.
// GPIOA on AHB2 (IOPORT) bus @ 0x42020000 (different from L4 @ 0x48000000).
// RCC @ 0x40021000 (same base as L4).
// Typical APB2 clock: 110 MHz.  BRR = 110 000 000 / 115 200 ≈ 955.

#include "../../hal/stm32/hal_stm32l5_uart.h"
#include <stdint.h>

#define USART1_BASE  0x40013800UL
#define GPIOA_BASE   0x42020000UL   // STM32L5 IOPORT bus (non-secure)
#define RCC_BASE     0x40021000UL

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

#define RCC_AHB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x4C))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x60))

#define GPIOA_MODER  (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH   (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define TICKS_PER_MS  110000u

static int l5_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while (!(USART_ISR & ISR_TXE) && t--) {}
        if (!(USART_ISR & ISR_TXE)) return HAL_UART_ERR;
        USART_TDR = data[i];
    }
    return HAL_UART_OK;
}
static int l5_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(USART_ISR & ISR_RXNE)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(USART_RDR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32l5_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    RCC_AHB2ENR |= (1UL << 0);   // GPIOAEN
    RCC_APB2ENR |= (1UL << 14);  // USART1EN
    // PA9 = TX, PA10 = RX → AF mode
    GPIOA_MODER = (GPIOA_MODER & ~(0x3UL << 18 | 0x3UL << 20))
                | (0x2UL << 18) | (0x2UL << 20);
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFFUL << 4))
               | (7UL << 4) | (7UL << 8);
    USART_CR1 = 0;
    USART_BRR = 955;   // 110 000 000 / 115 200
    USART_CR1 = CR1_TE | CR1_RE | CR1_UE;
    uart->send    = l5_send;
    uart->receive = l5_recv;
    return 0;
}
