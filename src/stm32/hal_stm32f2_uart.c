// STM32F2 USART1 HAL for LeafTS
// Reference: RM0033 (STM32F205/F207/F215/F217)
//
// USART v1 register layout (SR + DR — single data register), same as F4.
// USART1 @ 0x40011000, APB2.  PA9 = TX, PA10 = RX, AF7.
// Typical APB2 clock: 60 MHz (SYSCLK 120 MHz, APB2 prescaler /2).
// BRR = 60 000 000 / 115 200 ≈ 521.

#include "../../hal/stm32/hal_stm32f2_uart.h"
#include <stdint.h>

#define USART1_BASE  0x40011000UL

#define SR   (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define DR   (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define BRR  (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define CR1  (*(volatile uint32_t *)(USART1_BASE + 0x0C))

#define SR_RXNE  (1UL << 5)
#define SR_TXE   (1UL << 7)
#define CR1_RE   (1UL << 2)
#define CR1_TE   (1UL << 3)
#define CR1_UE   (1UL << 13)

// RCC — same base and bit layout as STM32F4
#define RCC_BASE      0x40023800UL
#define RCC_AHB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x44))
#define GPIOAEN       (1UL << 0)
#define USART1EN      (1UL << 4)

// GPIOA — STM32F2 uses MODER + AFRL/AFRH (same as F4)
#define GPIOA_BASE    0x40020000UL
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH    (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define TICKS_PER_MS  60000u

static int f2_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while (!(SR & SR_TXE) && t--) {}
        if (!(SR & SR_TXE)) return HAL_UART_ERR;
        DR = data[i];
    }
    return HAL_UART_OK;
}
static int f2_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(SR & SR_RXNE)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(DR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32f2_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    RCC_AHB1ENR |= GPIOAEN;
    RCC_APB2ENR |= USART1EN;
    // PA9 = TX, PA10 = RX → Alternate Function mode (MODER = 0b10)
    GPIOA_MODER = (GPIOA_MODER & ~(0x3UL << 18 | 0x3UL << 20))
                | (0x2UL << 18) | (0x2UL << 20);
    // AFRH: AF7 for PA9 (bits [7:4]) and PA10 (bits [11:8])
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFFUL << 4))
               | (7UL << 4) | (7UL << 8);
    CR1 = 0;
    BRR = 521;  // 60 000 000 / 115 200
    CR1 = CR1_TE | CR1_RE | CR1_UE;
    uart->send    = f2_send;
    uart->receive = f2_recv;
    return 0;
}
