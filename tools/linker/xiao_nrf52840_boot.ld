/* xiao_nrf52840_boot.ld
 *
 * Bootloader linker script for the MCUboot binary on nRF52840.
 * MCUboot occupies the first 64 KB of flash (0x00000000 – 0x0000FFFF).
 * After validating the application image it jumps to PRIMARY_SLOT_BASE + HEADER_SIZE.
 *
 * nRF52840: Cortex-M4F, 1 MB Flash @ 0x00000000, 256 KB RAM @ 0x20000000.
 * MCUboot itself uses only the first few KB of RAM.
 */

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
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

    /* 4 KB bootloader stack anchored at the top of its RAM region */
    .stack (NOLOAD) : ORIGIN(RAM) + LENGTH(RAM) - 4K {
        _stack_bottom = .;
        . += 4K;
        _stack_top = .;
    } > RAM

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.eh_frame*)
    }
}
