/* nrf52dk_nrf52832_boot.ld
 *
 * MCUboot linker script for the Nordic nRF52 DK (nRF52832).
 *
 * Layout:
 *   0x00000000 - 0x00017FFF  Bootloader            (96 KB)
 *   0x00018000 - 0x0003BFFF  Primary slot         (144 KB)
 *   0x0003C000 - 0x0005FFFF  Secondary slot       (144 KB)
 *   0x00060000 - 0x00067FFF  Scratch               (32 KB)
 *   0x00068000 - 0x0007FFFF  NVS / Calibration     (96 KB)
 */

MEMORY {
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 96K
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

    .stack ORIGIN(RAM) + LENGTH(RAM) - 0x1000 (NOLOAD) : {
        _stack_bottom = .;
        . += 0x1000;
        _stack_top = .;
    } > RAM

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
