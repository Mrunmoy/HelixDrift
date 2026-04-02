/* xiao_nrf52840_boot.ld
 *
 * Bootloader linker script for the MCUboot binary on nRF52840.
 * MCUboot occupies the first 96 KB of flash (0x00000000 – 0x00017FFF).
 * After validating the application image it jumps to PRIMARY_SLOT_BASE + HEADER_SIZE.
 *
 * nRF52840: Cortex-M4F, 1 MB Flash @ 0x00000000, 256 KB RAM @ 0x20000000.
 * MCUboot itself uses only the first few KB of RAM.
 */

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 96K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 32K   /* MCUboot only needs ~8-16 KB */
}

ENTRY(Reset_Handler)

SECTIONS {
    .isr_vector : {
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text : {
        *(.text*)
        *(.rodata*)
        *(.eh_frame*)
        . = ALIGN(4);
    } > FLASH

    .init_array : {
        _init_array_start = .;
        KEEP(*(.init_array*))
        _init_array_end = .;
        . = ALIGN(4);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data : {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    end = .;
    _end = .;

    /* 4 KB bootloader stack anchored at the top of its RAM region */
    . = ORIGIN(RAM) + LENGTH(RAM) - 0x1000;
    .stack (NOLOAD) : {
        _stack_bottom = .;
        . += 0x1000;
        _stack_top = .;
    } > RAM

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
