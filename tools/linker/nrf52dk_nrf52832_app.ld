/* nrf52dk_nrf52832_app.ld
 *
 * Development-board linker script for the Nordic nRF52 DK (nRF52832).
 *
 * This layout is for bring-up and generic nRF runtime work on the DK, not the
 * final product target. It mirrors the dual-slot OTA strategy used on the
 * nRF52840 branch but fits inside 512 KB flash:
 *
 *   0x00000000 - 0x0000FFFF  Bootloader            (64 KB)
 *   0x00010000 - 0x00037FFF  Primary slot         (160 KB)
 *   0x00038000 - 0x0005FFFF  Secondary slot       (160 KB)
 *   0x00060000 - 0x00067FFF  Scratch               (32 KB)
 *   0x00068000 - 0x0007FFFF  NVS / Calibration     (96 KB)
 *
 * App flash starts at PRIMARY_SLOT_BASE + MCUBOOT_HEADER_SIZE.
 */

MCUBOOT_HEADER_SIZE = 0x200;
PRIMARY_SLOT_BASE   = 0x00010000;
APP_FLASH_ORIGIN    = PRIMARY_SLOT_BASE + MCUBOOT_HEADER_SIZE;
APP_FLASH_LENGTH    = 160K - MCUBOOT_HEADER_SIZE;

MEMORY {
    FLASH (rx)  : ORIGIN = APP_FLASH_ORIGIN, LENGTH = APP_FLASH_LENGTH
    RAM   (rwx) : ORIGIN = 0x20000000,       LENGTH = 64K
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
