/* xiao_nrf52840_app.ld
 *
 * Application linker script for Seeed XIAO nRF52840 running behind MCUboot.
 *
 * Flash layout (1 MB):
 *   0x00000000 - 0x00017FFF  MCUboot bootloader    (96 KB)
 *   0x00018000 - 0x0006FFFF  Primary slot         (352 KB)
 *   0x00070000 - 0x000C7FFF  Secondary slot       (352 KB)
 *   0x000C8000 - 0x000CFFFF  Scratch               (32 KB)
 *   0x000D0000 - 0x000FFFFF  NVS / Calibration    (192 KB)
 *
 * The app binary is placed at PRIMARY_SLOT_BASE + MCUBOOT_HEADER_SIZE.
 * MCUboot header size: 0x200 (512 B), so app FLASH starts at 0x00010200.
 * imgtool.py prepends the header with --pad-header --header-size 0x200.
 *
 * nRF52840: Cortex-M4F, 1 MB Flash @ 0x00000000, 256 KB RAM @ 0x20000000.
 */

MCUBOOT_HEADER_SIZE = 0x200;
PRIMARY_SLOT_BASE   = 0x00018000;
APP_FLASH_ORIGIN    = PRIMARY_SLOT_BASE + MCUBOOT_HEADER_SIZE;
APP_FLASH_LENGTH    = 352K - MCUBOOT_HEADER_SIZE;

MEMORY {
    FLASH (rx)  : ORIGIN = APP_FLASH_ORIGIN, LENGTH = APP_FLASH_LENGTH
    RAM   (rwx) : ORIGIN = 0x20000000,       LENGTH = 256K
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

    /* 8 KB main stack anchored at the top of RAM */
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
