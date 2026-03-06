// STM32L4 USART2 HAL implementation.
// Registers from RM0394 §40.
// Works on real hardware AND in Renode (STM32F7_USART model emulates these regs).

#include "../hal/hal_stm32l4_uart.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// Register map (all offsets from USART2 base 0x40004400)
// ---------------------------------------------------------------------------
#define USART2_BASE  0x40004400UL

#define CR1  (*(volatile uint32_t *)(USART2_BASE + 0x00))  // Control reg 1
#define BRR  (*(volatile uint32_t *)(USART2_BASE + 0x0C))  // Baud rate reg
#define ISR  (*(volatile uint32_t *)(USART2_BASE + 0x1C))  // Status reg
#define RDR  (*(volatile uint32_t *)(USART2_BASE + 0x24))  // Receive data
#define TDR  (*(volatile uint32_t *)(USART2_BASE + 0x28))  // Transmit data

#define CR1_UE    (1UL << 0)   // USART enable
#define CR1_RE    (1UL << 2)   // Receiver enable
#define CR1_TE    (1UL << 3)   // Transmitter enable
#define ISR_RXNE  (1UL << 5)   // Read data register not empty
#define ISR_TXE   (1UL << 7)   // Transmit data register empty

// RCC APB1ENR1: bit 17 = USART2EN
#define RCC_APB1ENR1  (*(volatile uint32_t *)(0x40021000UL + 0x58))
#define USART2EN      (1UL << 17)

// GPIOA (0x48000000): PA2=TX AF7, PA3=RX AF7
#define GPIOA_MODER  (*(volatile uint32_t *)(0x48000000UL + 0x00))
#define GPIOA_AFRL   (*(volatile uint32_t *)(0x48000000UL + 0x20))
#define GPIOA_OSPEEDR (*(volatile uint32_t *)(0x48000000UL + 0x08))

// RCC AHB2ENR: bit 0 = GPIOAEN
#define RCC_AHB2ENR   (*(volatile uint32_t *)(0x40021000UL + 0x4C))
#define GPIOAEN       (1UL << 0)

// ---------------------------------------------------------------------------
// Timeout: Renode runs at 100 MIPS (set in .repl) = 100,000 ticks per ms.
// On real 80 MHz STM32L4: 80,000 ticks per ms (close enough for polling).
// ---------------------------------------------------------------------------
#define TICKS_PER_MS  100000UL

// ---------------------------------------------------------------------------
// Internal send/receive - match hal_uart_t function pointer signatures
// ---------------------------------------------------------------------------
static int uart_send(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        while (!(ISR & ISR_TXE)) {}   // wait until TX register empty
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
int stm32l4_uart_init(hal_uart_t *uart)
{
    if (!uart) return -1;

    // 1. Enable clocks: GPIOA + USART2
    RCC_AHB2ENR  |= GPIOAEN;
    RCC_APB1ENR1 |= USART2EN;

    // 2. Configure PA2 (TX) and PA3 (RX) as Alternate Function 7
    //    MODER bits [5:4]=10 (PA2 AF), bits [7:6]=10 (PA3 AF)
    GPIOA_MODER  &= ~((3UL << 4) | (3UL << 6));
    GPIOA_MODER  |=  ((2UL << 4) | (2UL << 6));
    //    AFRL: AF7=0x7 for PA2 [11:8] and PA3 [15:12]
    GPIOA_AFRL   &= ~((0xFUL << 8) | (0xFUL << 12));
    GPIOA_AFRL   |=  ((7UL  << 8) | (7UL  << 12));
    //    High speed on PA2, PA3
    GPIOA_OSPEEDR|=  ((3UL << 4) | (3UL << 6));

    // 3. Configure USART2: 115200 baud @ 80 MHz PCLK
    //    BRR = PCLK / baud = 80000000 / 115200 = 694
    CR1 = 0;            // disable while configuring
    BRR = 694UL;
    CR1 = CR1_UE | CR1_TE | CR1_RE;  // enable USART, TX and RX

    uart->send    = uart_send;
    uart->receive = uart_receive;
    return 0;
}
