/* STM32L476: 1 MB Flash @ 0x08000000, 96 KB SRAM @ 0x20000000 */
MEMORY {
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 96K
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);
ENTRY(Reset_Handler)

SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) } >FLASH

    .text : {
        *(.text*)
        *(.rodata*)
    } >FLASH

    _sidata = LOADADDR(.data);
    .data : {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >SRAM AT >FLASH

    .bss (NOLOAD) : {
        _sbss = .;
        *(.bss*) *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >SRAM

    /* Heap starts right after BSS - required by newlib _sbrk */
    end = _ebss;

    /* ARM exception index - required by libgcc unwind */
    .ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH
}
