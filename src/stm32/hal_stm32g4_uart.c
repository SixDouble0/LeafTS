// STM32G4 USART1 HAL implementation.
// Reference: RM0440 §39 (USART).
//
// Pinout: PA9 = TX, PA10 = RX, AF7, 115200 8N1
// Clock : 170 MHz PCLK2 (maximum — no APB2 prescaler at full speed)
// USART v2 register layout (ISR/RDR/TDR) — identical to STM32L4.
// GPIO base: 0x48000000 (AHB2 domain, same as L4).
// RCC base : 0x40021000 (same as G0/L4).

#include "../../hal/stm32/hal_stm32g4_uart.h"
#include <stdint.h>

#define USART1_BASE  0x40013800UL
#define CR1  (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define BRR  (*(volatile uint32_t *)(USART1_BASE + 0x0C))
#define ISR  (*(volatile uint32_t *)(USART1_BASE + 0x1C))
#define RDR  (*(volatile uint32_t *)(USART1_BASE + 0x24))
#define TDR  (*(volatile uint32_t *)(USART1_BASE + 0x28))

#define CR1_UE  (1UL << 0)
#define CR1_RE  (1UL << 2)
#define CR1_TE  (1UL << 3)
#define ISR_RXNE (1UL << 5)
#define ISR_TXE  (1UL << 7)

#define RCC_BASE      0x40021000UL
#define RCC_AHB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x4C))
#define RCC_APB2ENR   (*(volatile uint32_t *)(RCC_BASE + 0x60))
#define GPIOAEN       (1UL <<  0)
#define USART1EN      (1UL << 14)

#define GPIOA_BASE    0x48000000UL
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_AFRH    (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

// BRR = PCLK2 / baud = 170 000 000 / 115200 = 1476
#define BRR_115200_170MHZ  1476UL
#define TICKS_PER_MS       170000UL

static int uart_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) { while (!(ISR & ISR_TXE)) {} TDR = data[i]; }
    return HAL_UART_OK;
}
static int uart_receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 0;
        while (!(ISR & ISR_RXNE)) { if (++t >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(RDR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32g4_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    RCC_AHB2ENR |= GPIOAEN;
    RCC_APB2ENR |= USART1EN;
    GPIOA_MODER &= ~((3UL << 18) | (3UL << 20));
    GPIOA_MODER |=  ((2UL << 18) | (2UL << 20));  // AF mode PA9/PA10
    GPIOA_AFRH  &= ~((0xFUL << 4) | (0xFUL << 8));
    GPIOA_AFRH  |=  ((7UL   << 4) | (7UL   << 8));  // AF7
    GPIOA_OSPEEDR |= ((3UL << 18) | (3UL << 20));
    CR1 = 0;
    BRR = BRR_115200_170MHZ;
    CR1 = CR1_UE | CR1_TE | CR1_RE;
    uart->send    = uart_send;
    uart->receive = uart_receive;
    return 0;
}
