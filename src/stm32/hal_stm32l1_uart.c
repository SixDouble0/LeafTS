// STM32L1 USART1 HAL for LeafTS
// Reference: RM0038 (STM32L151/L152/L162)
//
// USART v1 register layout (SR + DR — single data register).
// USART1 @ 0x40013800, APB2.  PA9 = TX, PA10 = RX, AF7.
// GPIO uses MODER-based config (unlike STM32F1 which uses CRH).
// GPIOA @ 0x40020000 (AHBENR, GPIOAEN = bit 0 of RCC_AHBENR).
// RCC_AHBENR @ 0x4002381C (bit 0 = GPIOAEN).
// Typical SYSCLK: 32 MHz.  BRR = 32 000 000 / 115 200 ≈ 278.

#include "../../hal/stm32/hal_stm32l1_uart.h"
#include <stdint.h>

#define USART1_BASE  0x40013800UL
#define GPIOA_BASE   0x40020000UL
#define RCC_BASE     0x40023800UL

// USART v1 registers (same layout as STM32F1)
#define SR   (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define DR   (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define BRR  (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define CR1  (*(volatile uint32_t *)(USART1_BASE + 0x0C))

#define SR_RXNE  (1UL << 5)
#define SR_TXE   (1UL << 7)
#define CR1_RE   (1UL << 2)
#define CR1_TE   (1UL << 3)
#define CR1_UE   (1UL << 13)

// RCC: AHBENR for GPIOA (bit 0), APB2ENR for USART1 (bit 14)
// RM0038: RCC_AHBENR at offset 0x1C, GPIOAEN at bit 0
#define RCC_AHBENR   (*(volatile uint32_t *)(RCC_BASE + 0x1C))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x44))

// GPIOA: MODER-based (like L4/F4, different from F1 which uses CRH)
#define GPIOA_MODER  (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH   (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define TICKS_PER_MS  32000u

static int l1_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while (!(SR & SR_TXE) && t--) {}
        if (!(SR & SR_TXE)) return HAL_UART_ERR;
        DR = data[i];
    }
    return HAL_UART_OK;
}
static int l1_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(SR & SR_RXNE)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(DR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32l1_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    // RM0038: GPIOAEN is at bit 0 of RCC_AHBENR
    RCC_AHBENR  |= (1UL << 0);   // GPIOAEN
    RCC_APB2ENR |= (1UL << 14);  // USART1EN
    // PA9 = TX, PA10 = RX → Alternate Function mode (MODER = 0b10)
    GPIOA_MODER = (GPIOA_MODER & ~(0x3UL << 18 | 0x3UL << 20))
                | (0x2UL << 18) | (0x2UL << 20);
    // AFRH: AF7 for PA9 (bits [7:4]) and PA10 (bits [11:8])
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFFUL << 4))
               | (7UL << 4) | (7UL << 8);
    CR1 = 0;
    BRR = 278;   // 32 000 000 / 115 200
    CR1 = CR1_TE | CR1_RE | CR1_UE;
    uart->send    = l1_send;
    uart->receive = l1_recv;
    return 0;
}
