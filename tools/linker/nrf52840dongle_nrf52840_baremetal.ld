/* nrf52840dongle_nrf52840_baremetal.ld
 *
 * Standalone bring-up linker script for the Nordic nRF52840 Dongle (PCA10059).
 *
 * This target is intended for direct SWD flashing during bring-up. It uses the
 * full nRF52840 address space rather than the factory USB bootloader layout.
 *
 * nRF52840: Cortex-M4F, 1 MB Flash @ 0x00000000, 256 KB RAM @ 0x20000000.
 */

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
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

    . = ORIGIN(RAM) + LENGTH(RAM) - 0x2000;
    .stack (NOLOAD) : {
        _stack_bottom = .;
        . += 0x2000;
        _stack_top = .;
    } > RAM

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
