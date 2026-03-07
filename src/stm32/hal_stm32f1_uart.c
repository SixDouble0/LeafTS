// STM32F1 USART1 HAL implementation.
// Reference: RM0008 §27 (USART).
// Works on real hardware (Blue Pill, Nucleo-F103RB, etc.)
// and in Renode (STM32F1 USART model emulates these registers).
//
// Pinout: PA9 = TX (AF push-pull), PA10 = RX (input floating)
// Baud  : 115200 @ 72 MHz APB2 (standard HSE 8 MHz + PLL ×9)

#include "../../hal/stm32/hal_stm32f1_uart.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// Register map
// Note: STM32F1 USART uses "v1" register layout (SR + DR) unlike STM32L4 (ISR + RDR + TDR).
// ---------------------------------------------------------------------------
#define USART1_BASE  0x40013800UL

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
// RCC — enable GPIOA (APB2ENR bit 2) and USART1 (APB2ENR bit 14)
// ---------------------------------------------------------------------------
#define RCC_APB2ENR  (*(volatile uint32_t *)(0x40021000UL + 0x18))
#define IOPAEN       (1UL <<  2)
#define USART1EN     (1UL << 14)

// ---------------------------------------------------------------------------
// GPIOA — configure PA9 (TX) and PA10 (RX)
// STM32F1 GPIO uses CRL (pins 0-7) and CRH (pins 8-15).
// Each pin has 4 bits: MODE[1:0] + CNF[1:0]
//   PA9  (TX): MODE=11 (output 50 MHz), CNF=10 (AF push-pull)  → 0b1011 = 0xB
//   PA10 (RX): MODE=00 (input),         CNF=01 (floating input) → 0b0100 = 0x4
// ---------------------------------------------------------------------------
#define GPIOA_CRH  (*(volatile uint32_t *)(0x40010800UL + 0x04))
//                  CRH bit positions: PA8=[3:0] PA9=[7:4] PA10=[11:8] ...

// ---------------------------------------------------------------------------
// Baud rate: BRR = PCLK2 / baud = 72 000 000 / 115200 = 625
// ---------------------------------------------------------------------------
#define BRR_115200_72MHZ  625UL

// ---------------------------------------------------------------------------
// Timeout
// ---------------------------------------------------------------------------
#define TICKS_PER_MS  72000UL   // ~72 000 ticks per ms at 72 MHz

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
int stm32f1_uart_init(hal_uart_t *uart)
{
    if (!uart) return -1;

    // 1. Enable APB2 clocks: GPIOA + USART1
    RCC_APB2ENR |= IOPAEN | USART1EN;

    // 2. Configure PA9 (TX) as AF push-pull 50 MHz, PA10 (RX) as floating input
    //    CRH controls pins 8-15.  We must preserve bits for PA8, PA11-PA15.
    GPIOA_CRH &= ~((0xFUL << 4) | (0xFUL << 8));  // clear PA9 [7:4] and PA10 [11:8]
    GPIOA_CRH |=  ((0xBUL << 4) | (0x4UL << 8));  // PA9=AF PP 50MHz, PA10=input float

    // 3. Configure USART1: 115200 baud, 8N1
    CR1 = 0;                             // disable while configuring
    BRR = BRR_115200_72MHZ;
    CR1 = CR1_UE | CR1_TE | CR1_RE;     // enable USART, TX, RX

    uart->send    = uart_send;
    uart->receive = uart_receive;
    return 0;
}
