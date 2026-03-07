// STM32WB USART1 HAL for LeafTS
// Reference: RM0434 (STM32WB55/WB35)
//
// USART v2 register layout (CR1/BRR/ISR/RDR/TDR).
// USART1 @ 0x40013800, APB2.  PA9 = TX, PA10 = RX, AF7.
// GPIOA @ 0x48000000 (AHB2), RCC @ 0x58000000.
// Typical clock: 64 MHz.  BRR = 64 000 000 / 115 200 ≈ 556.

#include "../../hal/stm32/hal_stm32wb_uart.h"
#include <stdint.h>

#define USART1_BASE  0x40013800UL
#define GPIOA_BASE   0x48000000UL
#define RCC_BASE     0x58000000UL

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

#define TICKS_PER_MS  64000u

static int wb_send(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint32_t t = 10u * TICKS_PER_MS;
        while (!(USART_ISR & ISR_TXE) && t--) {}
        if (!(USART_ISR & ISR_TXE)) return HAL_UART_ERR;
        USART_TDR = data[i];
    }
    return HAL_UART_OK;
}
static int wb_recv(uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
    uint32_t deadline = timeout_ms * TICKS_PER_MS;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(USART_ISR & ISR_RXNE)) { if (++ticks >= deadline) return HAL_UART_TIMEOUT; }
        buf[i] = (uint8_t)(USART_RDR & 0xFF);
    }
    return HAL_UART_OK;
}

int stm32wb_uart_init(hal_uart_t *uart) {
    if (!uart) return -1;
    RCC_AHB2ENR |= (1UL << 0);   // GPIOAEN
    RCC_APB2ENR |= (1UL << 14);  // USART1EN
    // PA9 = TX, PA10 = RX → AF mode
    GPIOA_MODER = (GPIOA_MODER & ~(0x3UL << 18 | 0x3UL << 20))
                | (0x2UL << 18) | (0x2UL << 20);
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFFUL << 4))
               | (7UL << 4) | (7UL << 8);
    USART_CR1 = 0;
    USART_BRR = 556;   // 64 000 000 / 115 200
    USART_CR1 = CR1_TE | CR1_RE | CR1_UE;
    uart->send    = wb_send;
    uart->receive = wb_recv;
    return 0;
}
