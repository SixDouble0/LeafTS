// STM32F4 USART1 HAL implementation.
// Reference: RM0090 §30 (USART).
// Works on real hardware (STM32F4 Discovery, Nucleo-F411RE, etc.)
// and in Renode (STM32F4 USART model emulates these registers).
//
// Pinout: PA9 = TX, PA10 = RX, AF7, 115200 8N1
// Baud  : 115200 @ 84 MHz APB2 (standard PLL: 168 MHz SYSCLK, APB2 prescaler /2)
//
// Note: STM32F4 uses "USART v1" register layout (SR + DR — single data register)
//       unlike STM32L4/G0 which use "USART v2" (ISR + RDR + TDR).

#include "../../hal/stm32/hal_stm32f4_uart.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// USART1 register map (RM0090 §30.6)
// ---------------------------------------------------------------------------
#define USART1_BASE  0x40011000UL

#define SR   (*(volatile uint32_t *)(USART1_BASE + 0x00))  // Status register
#define DR   (*(volatile uint32_t *)(USART1_BASE + 0x04))  // Data register (RX + TX)
#define BRR  (*(volatile uint32_t *)(USART1_BASE + 0x08))  // Baud rate register
#define CR1  (*(volatile uint32_t *)(USART1_BASE + 0x0C))  // Control register 1

#define SR_RXNE  (1UL << 5)   // Read data register not empty
#define SR_TXE   (1UL << 7)   // Transmit data register empty

#define CR1_RE   (1UL << 2)   // Receiver enable
#define CR1_TE   (1UL << 3)   // Transmitter enable
#define CR1_UE   (1UL << 13)  // USART enable

// ---------------------------------------------------------------------------
// RCC
// ---------------------------------------------------------------------------
#define RCC_AHB1ENR   (*(volatile uint32_t *)(0x40023800UL + 0x30))
#define RCC_APB2ENR   (*(volatile uint32_t *)(0x40023800UL + 0x44))
#define GPIOAEN       (1UL << 0)
#define USART1EN      (1UL << 4)

// ---------------------------------------------------------------------------
// GPIOA — STM32F4 uses MODER + AFRH (same layout as STM32L4)
// ---------------------------------------------------------------------------
#define GPIOA_BASE    0x40020000UL
#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_AFRH    (*(volatile uint32_t *)(GPIOA_BASE + 0x24))  // pins 8-15

// PA9  → AFRH bits [7:4],  PA10 → AFRH bits [11:8]
// USART1 = AF7 on STM32F4

// ---------------------------------------------------------------------------
// Baud rate: BRR = PCLK2 / baud = 84 000 000 / 115200 ≈ 729
// ---------------------------------------------------------------------------
#define BRR_115200_84MHZ  729UL

// ---------------------------------------------------------------------------
// Timeout
// ---------------------------------------------------------------------------
#define TICKS_PER_MS  84000UL   // ~84 000 ticks per ms at 84 MHz

// ---------------------------------------------------------------------------
// Internal send / receive
// ---------------------------------------------------------------------------
static int uart_send(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        while (!(SR & SR_TXE)) {}  // wait for TX register empty
        DR = data[i];
    }
    return HAL_UART_OK;
}

static int uart_receive(uint8_t *buf, uint32_t len, uint32_t timeout_ms)
{
    uint32_t deadline = timeout_ms * TICKS_PER_MS;

    for (uint32_t i = 0; i < len; i++) {
        uint32_t ticks = 0;
        while (!(SR & SR_RXNE)) {
            if (++ticks >= deadline) return HAL_UART_TIMEOUT;
        }
        buf[i] = (uint8_t)(DR & 0xFF);
    }
    return HAL_UART_OK;
}

// ---------------------------------------------------------------------------
// Public init
// ---------------------------------------------------------------------------
int stm32f4_uart_init(hal_uart_t *uart)
{
    if (!uart) return -1;

    // 1. Enable AHB1 clock for GPIOA and APB2 clock for USART1
    RCC_AHB1ENR |= GPIOAEN;
    RCC_APB2ENR |= USART1EN;

    // 2. Configure PA9 and PA10 as Alternate Function (MODER=10)
    GPIOA_MODER &= ~((3UL << 18) | (3UL << 20));  // clear PA9 [19:18] and PA10 [21:20]
    GPIOA_MODER |=  ((2UL << 18) | (2UL << 20));  // AF mode

    // 3. Select AF7 (USART1) for PA9 and PA10 in AFRH
    GPIOA_AFRH  &= ~((0xFUL << 4) | (0xFUL << 8));
    GPIOA_AFRH  |=  ((7UL   << 4) | (7UL   << 8));

    // 4. High speed on PA9, PA10
    GPIOA_OSPEEDR |= ((3UL << 18) | (3UL << 20));

    // 5. Configure USART1: 115200 baud, 8N1
    CR1 = 0;                           // disable while configuring
    BRR = BRR_115200_84MHZ;
    CR1 = CR1_UE | CR1_TE | CR1_RE;   // enable USART, TX, RX

    uart->send    = uart_send;
    uart->receive = uart_receive;
    return 0;
}
