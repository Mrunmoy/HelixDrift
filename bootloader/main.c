/* main.c — MCUboot standalone entry point for XIAO nRF52840.
 *
 * Execution flow:
 *   1. startup_nrf52840.S runs Reset_Handler (copies .data, zeros .bss).
 *   2. main() calls boot_go() which validates and optionally overwrites the
 *      primary slot from the secondary slot.
 *   3. On success, main() sets VTOR and jumps to the application reset vector.
 *   4. On failure (no valid image), a fault LED is toggled forever.
 *
 * The application image header is MCUBOOT_IMAGE_HEADER_SIZE (0x200) bytes,
 * so the app vectors start at the configured primary slot base + 0x200.
 */

#include "bootutil/bootutil.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"

/* nRF52840 SCB->VTOR address */
#define SCB_VTOR  (*((volatile uint32_t*)0xE000ED08u))

#ifndef HELIX_FAULT_LED_PIN
#define HELIX_FAULT_LED_PIN 26u
#endif

#ifndef HELIX_BOOT_UART_TX_PIN
#define HELIX_BOOT_UART_TX_PIN 0xFFFFFFFFu
#endif

#ifndef HELIX_BOOT_UART_RX_PIN
#define HELIX_BOOT_UART_RX_PIN 0xFFFFFFFFu
#endif

/* Fault indicator: board-specific LED on port 0 (active-low by convention).
 * GPIO registers for direct register access without nrfx dependency. */
#define GPIO_P0_BASE        0x50000000u
#define GPIO_DIRSET_OFFSET  0x518u
#define GPIO_OUTSET_OFFSET  0x508u
#define GPIO_OUTCLR_OFFSET  0x50Cu

#define UART0_BASE              0x40002000u
#define UART_TASKS_STARTRX      0x000u
#define UART_TASKS_STARTTX      0x008u
#define UART_EVENTS_TXDRDY      0x11Cu
#define UART_ENABLE             0x500u
#define UART_PSELTXD            0x50Cu
#define UART_PSELRXD            0x514u
#define UART_PSELRTS            0x504u
#define UART_PSELCTS            0x510u
#define UART_BAUDRATE           0x524u
#define UART_CONFIG             0x56Cu

static inline volatile uint32_t* uart_reg(uint32_t off) {
    return (volatile uint32_t*)(UART0_BASE + off);
}

static inline void fault_led_init(void) {
    volatile uint32_t* dirset = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_DIRSET_OFFSET);
    *dirset = (1u << HELIX_FAULT_LED_PIN);
}

static inline void fault_led_on(void) {
    volatile uint32_t* outclr = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_OUTCLR_OFFSET);
    *outclr = (1u << HELIX_FAULT_LED_PIN);  /* active-low: clear = LED on */
}

static inline void fault_led_off(void) {
    volatile uint32_t* outset = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_OUTSET_OFFSET);
    *outset = (1u << HELIX_FAULT_LED_PIN);  /* active-low: set = LED off */
}

/* Simple spin delay. nRF52840 boots on 16 MHz RC oscillator.
 * Each loop body is ~3 cycles at 16 MHz ≈ ~187 ns/iteration.
 * 80000 iterations ≈ ~15 ms. */
static void spin_delay(volatile uint32_t ticks) {
    while (ticks--) { __asm__ volatile("nop"); }
}

static inline int boot_uart_enabled(void) {
    return HELIX_BOOT_UART_TX_PIN != 0xFFFFFFFFu;
}

static void boot_uart_init(void) {
    if (!boot_uart_enabled()) {
        return;
    }
    *uart_reg(UART_ENABLE) = 0u;
    *uart_reg(UART_PSELTXD) = HELIX_BOOT_UART_TX_PIN;
    *uart_reg(UART_PSELRXD) = HELIX_BOOT_UART_RX_PIN;
    *uart_reg(UART_PSELRTS) = 0xFFFFFFFFu;
    *uart_reg(UART_PSELCTS) = 0xFFFFFFFFu;
    *uart_reg(UART_BAUDRATE) = 0x01D7E000u; /* 115200 */
    *uart_reg(UART_CONFIG) = 0u;
    *uart_reg(UART_ENABLE) = 4u;
    *uart_reg(UART_TASKS_STARTTX) = 1u;
    *uart_reg(UART_TASKS_STARTRX) = 1u;
}

static void boot_uart_write_byte(uint8_t value) {
    if (!boot_uart_enabled()) {
        return;
    }
    *uart_reg(UART_EVENTS_TXDRDY) = 0u;
    *((volatile uint32_t*)(UART0_BASE + 0x51Cu)) = value;
    while (*uart_reg(UART_EVENTS_TXDRDY) == 0u) {}
}

static void boot_uart_write_str(const char* s) {
    if (!boot_uart_enabled()) {
        return;
    }
    while (*s) {
        if (*s == '\n') {
            boot_uart_write_byte('\r');
        }
        boot_uart_write_byte((uint8_t)*s++);
    }
}

static void boot_uart_write_hex32(uint32_t value) {
    static const char kHex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {
        boot_uart_write_byte((uint8_t)kHex[(value >> (i * 4)) & 0xFu]);
    }
}

int main(void) {
    boot_uart_init();
    boot_uart_write_str("mcuboot start\n");
    boot_uart_write_str("swap=");
    boot_uart_write_hex32((uint32_t)boot_swap_type());
    boot_uart_write_str("\n");

    struct boot_rsp rsp = {0};
    int rc = boot_go(&rsp);

    boot_uart_write_str("boot_go rc=");
    boot_uart_write_hex32((uint32_t)rc);
    boot_uart_write_str(" off=");
    boot_uart_write_hex32(rsp.br_image_off);
    if (rsp.br_hdr != 0) {
        boot_uart_write_str(" ver=");
        boot_uart_write_hex32((uint32_t)rsp.br_hdr->ih_ver.iv_major);
        boot_uart_write_byte('.');
        boot_uart_write_hex32((uint32_t)rsp.br_hdr->ih_ver.iv_minor);
    }
    boot_uart_write_str("\n");

    if (rc != 0) {
        /* No valid image — blink fault LED forever. */
        fault_led_init();
        boot_uart_write_str("fault\n");
        while (1) {
            fault_led_on();
            spin_delay(80000u);   /* ~15 ms on */
            fault_led_off();
            spin_delay(80000u);   /* ~15 ms off */
        }
    }

    /* Compute the address of the application's vector table.
     * rsp.br_image_off is the primary slot base from the flash map.
     * rsp.br_hdr->ih_hdr_size is MCUBOOT_IMAGE_HEADER_SIZE (0x200). */
    uint32_t app_vector_table = rsp.br_image_off + rsp.br_hdr->ih_hdr_size;

    /* Read initial stack pointer and reset vector from the app vector table. */
    uint32_t app_sp = *((uint32_t*)app_vector_table);
    uint32_t app_pc = *((uint32_t*)(app_vector_table + 4u));

    boot_uart_write_str("jump vt=");
    boot_uart_write_hex32(app_vector_table);
    boot_uart_write_str(" pc=");
    boot_uart_write_hex32(app_pc);
    boot_uart_write_str("\n");

    /* Relocate VTOR to the application's vector table.
     * Disable interrupts first to close the window between the VTOR write
     * and the stack/PC switch.  DSB + ISB ensure the new table is visible
     * to the exception mechanism before we hand off (Cortex-M4 TRM §4.2.2). */
    __asm__ volatile("cpsid i" ::: "memory");
    SCB_VTOR = app_vector_table;
    __asm__ volatile("dsb" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    /* Set the main stack pointer and jump to the application. */
    __asm__ volatile(
        "msr msp, %0  \n"
        "bx  %1       \n"
        :
        : "r"(app_sp), "r"(app_pc)
        : "memory"
    );

    /* Unreachable */
    while (1) {}
    return 0;
}
