// STM32F3 USART1 HAL implementation.
// Reference: RM0316 §29 (USART).
//
// Pinout: PA9 = TX, PA10 = RX, AF7, 115200 8N1 @ 72 MHz APB2
// Register layout: identical to STM32F1 (SR/DR/BRR/CR1 at same offsets).
// GPIO: uses CRL/CRH like STM32F1 (not MODER/AFRH).

#include "../../hal/stm32/hal_stm32f3_uart.h"
#include <stdint.h>

#define USART1_BASE  0x40013800UL
#define SR   (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define DR   (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define BRR  (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define CR1  (*(volatile uint32_t *)(USART1_BASE + 0x0C))

#define SR_RXNE  (1UL << 5)
#define SR_TXE   (1UL << 7)
#define CR1_RE   (1UL << 2)
#define CR1_TE   (1UL << 3)
#define CR1_UE   (1UL << 13)

// RCC APB2ENR — same base as F1 (0x40021000)
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40021000UL + 0x18))
#define IOPAEN       (1UL <<  2)
#define USART1EN     (1UL << 14)

// GPIOA CRH — F3 still uses CRL/CRH like F1
#define GPIOA_CRH  (*(volatile uint32_t *)(0x48000000UL + 0x04))
// NOTE: F3 GPIO base is 0x48000000, not 0x40010800 like F1!
// PA9=[7:4]: AF push-pull 50 MHz → 0xB
// PA10=[11:8]: input floating → 0x4

#define BRR_115200_72MHZ  625UL
#define TICKS_PER_MS      72000UL

static int uart_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) { while (!(SR & SR_TXE)) {} DR = data[i]; }
    return HAL_UART_OK;
}
static int uart_receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 0;
        while (!(SR & SR_RXNE)) { if (++t >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(DR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32f3_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    RCC_APB2ENR |= IOPAEN | USART1EN;
    GPIOA_CRH &= ~((0xFUL << 4) | (0xFUL << 8));
    GPIOA_CRH |=  ((0xBUL << 4) | (0x4UL << 8));  // PA9=AF PP 50MHz, PA10=float input
    CR1 = 0;
    BRR = BRR_115200_72MHZ;
    CR1 = CR1_UE | CR1_TE | CR1_RE;
    uart->send    = uart_send;
    uart->receive = uart_receive;
    return 0;
}
