// STM32G0 USART1 HAL implementation.
// Reference: RM0444 §31 (USART).
//
// Pinout : PA9 = TX, PA10 = RX, AF1, 115200 8N1
// Clock  : 16 MHz HSI (reset default).  BRR = 16 000 000 / 115200 = 139.
//          If a PLL is configured, update BRR accordingly:
//            BRR = PCLK / 115200  (e.g. 64 MHz → 556, 48 MHz → 417)
//
// Key differences vs STM32L4:
//   - USART1 uses AF1 (not AF7) on STM32G0
//   - GPIO base is 0x50000000 (not 0x48000000)
//   - RCC_IOPENR  at 0x40021000 (GPIO clock enable)
//   - RCC_APBENR2 at 0x40021018 (USART1 clock enable)
// USART register layout (ISR/RDR/TDR) is identical to STM32L4.

#include "../hal/hal_stm32g0_uart.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// USART1 register map — USART v2 (same layout as STM32L4 / STM32G4)
// ---------------------------------------------------------------------------
#define USART1_BASE  0x40013800UL

#define CR1  (*(volatile uint32_t *)(USART1_BASE + 0x00))  // Control register 1
#define BRR  (*(volatile uint32_t *)(USART1_BASE + 0x0C))  // Baud rate register
#define ISR  (*(volatile uint32_t *)(USART1_BASE + 0x1C))  // Status register
#define RDR  (*(volatile uint32_t *)(USART1_BASE + 0x24))  // Receive data register
#define TDR  (*(volatile uint32_t *)(USART1_BASE + 0x28))  // Transmit data register

#define CR1_RE   (1UL << 2)
#define CR1_TE   (1UL << 3)
#define CR1_UE   (1UL << 0)   // Note: on G0/G4 UE is bit 0, same as L4
#define ISR_RXNE (1UL << 5)
#define ISR_TXE  (1UL << 7)

// ---------------------------------------------------------------------------
// RCC (RM0444 §5.4)
// ---------------------------------------------------------------------------
#define RCC_BASE      0x40021000UL
#define RCC_IOPENR    (*(volatile uint32_t *)(RCC_BASE + 0x00))  // GPIO clock enable
#define RCC_APBENR2   (*(volatile uint32_t *)(RCC_BASE + 0x18))  // APB2 peripheral enable
#define IOPAEN        (1UL <<  0)
#define USART1EN      (1UL << 14)

// ---------------------------------------------------------------------------
// GPIOA (STM32G0 GPIO base is 0x50000000)
// ---------------------------------------------------------------------------
#define GPIOA_BASE    0x50000000UL
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_AFRH    (*(volatile uint32_t *)(GPIOA_BASE + 0x24))  // pins 8-15

// PA9 = AFRH bits [7:4], PA10 = AFRH bits [11:8]
// USART1 on STM32G0 = AF1  (not AF7!)

// ---------------------------------------------------------------------------
// Baud rate: 16 MHz HSI / 115200 = 139
// ---------------------------------------------------------------------------
#define BRR_115200_16MHZ  139UL

// ---------------------------------------------------------------------------
// Timeout
// ---------------------------------------------------------------------------
#define TICKS_PER_MS  16000UL   // ~16 000 ticks per ms at 16 MHz HSI

// ---------------------------------------------------------------------------
// Internal send / receive
// ---------------------------------------------------------------------------
static int uart_send(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        while (!(ISR & ISR_TXE)) {}
        TDR = data[i];
    }
    return HAL_UART_OK;
}

static int uart_receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    uint32_t deadline = timeout_ms * TICKS_PER_MS;

    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(ISR & ISR_RXNE)) {
            if (++ticks >= deadline) return HAL_UART_TIMEOUT;
        }
        buf[i] = (uint8_t)(RDR & 0xFF);
    }
    return HAL_UART_OK;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int stm32g0_uart_init(hal_uart_t *uart)
{
    if (!uart) return -1;

    // 1. Enable GPIO A and USART1 clocks
    RCC_IOPENR  |= IOPAEN;
    RCC_APBENR2 |= USART1EN;

    // 2. Set PA9 and PA10 to Alternate Function mode (MODER=10)
    GPIOA_MODER &= ~((3UL << 18) | (3UL << 20));  // PA9 [19:18], PA10 [21:20]
    GPIOA_MODER |=  ((2UL << 18) | (2UL << 20));

    // 3. Select AF1 (USART1 on STM32G0) for PA9 and PA10
    GPIOA_AFRH  &= ~((0xFUL << 4) | (0xFUL << 8));
    GPIOA_AFRH  |=  ((1UL   << 4) | (1UL   << 8));  // AF1, not AF7!

    // 4. High speed on PA9, PA10
    GPIOA_OSPEEDR |= ((3UL << 18) | (3UL << 20));

    // 5. Configure USART1: 115200 @ 16 MHz HSI, 8N1
    CR1 = 0;
    BRR = BRR_115200_16MHZ;
    CR1 = CR1_UE | CR1_TE | CR1_RE;

    uart->send    = uart_send;
    uart->receive = uart_receive;
    return 0;
}
