// Minimal Cortex-M4 startup for Renode.
// Runs before main(): sets up stack, copies .data, zeroes .bss,
// enables semihosting (so Unity's printf reaches the Renode console).

#include <stdint.h>

// Symbols from stm32l476.ld
extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata;  // .data: LMA, VMA start, VMA end
extern uint32_t _sbss, _ebss;

extern int main(void);

void Reset_Handler(void);

// Vector table: only SP + Reset needed for Renode to start
__attribute__((section(".isr_vector"), used))
static const uint32_t VectorTable[2] = {
    (uint32_t)(uintptr_t)&_estack,
    (uint32_t)(uintptr_t)Reset_Handler,
};

__attribute__((noreturn)) void Reset_Handler(void) {
    // Copy .data from flash to RAM
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;

    // Zero .bss
    for (uint32_t *p = &_sbss; p < &_ebss; p++) *p = 0;

    main();
    for (;;) {}
}

// ---------------------------------------------------------------------------
// Redirect printf/puts to USART2 (Renode shows it in showAnalyzer window).
// STM32L4 USART2 base = 0x40004400
//   ISR offset 0x1C: bit 7 = TXE (transmit data register empty)
//   TDR offset 0x28: write byte here to send
// ---------------------------------------------------------------------------
#define USART2_BASE  0x40004400UL
#define USART2_ISR   (*(volatile uint32_t *)(USART2_BASE + 0x1C))
#define USART2_TDR   (*(volatile uint32_t *)(USART2_BASE + 0x28))
#define USART2_CR1   (*(volatile uint32_t *)(USART2_BASE + 0x00))

int _write(int fd, const char *buf, int len) {
    (void)fd;
    // Enable USART2: UE (bit0) + TE (bit3)
    USART2_CR1 |= (1u << 0) | (1u << 3);
    for (int i = 0; i < len; i++) {
        while (!(USART2_ISR & (1u << 7))) {}  // wait TXE
        USART2_TDR = (uint8_t)buf[i];
    }
    return len;
}
