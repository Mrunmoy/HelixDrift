/* nrf52dk_nrf52832_baremetal.ld
 *
 * Standalone bring-up linker script for the Nordic nRF52 DK (nRF52832).
 *
 * Unlike the OTA-slot development layout, this script boots directly from
 * address 0x00000000 so the image can run on a bare DK without MCUboot.
 */

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
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

    .noinit (NOLOAD) : {
        *(.noinit*)
        . = ALIGN(4);
    } > RAM

    .stack ORIGIN(RAM) + LENGTH(RAM) - 0x2000 (NOLOAD) : {
        _stack_bottom = .;
        . += 0x2000;
        _stack_top = .;
    } > RAM

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
