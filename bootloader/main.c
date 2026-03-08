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
 * so the app vectors start at PRIMARY_SLOT_BASE + 0x200.
 */

#include "bootutil/bootutil.h"
#include "bootutil/image.h"

/* nRF52840 SCB->VTOR address */
#define SCB_VTOR  (*((volatile uint32_t*)0xE000ED08u))

/* Fault indicator: XIAO nRF52840 built-in LED on P0.26 (active-low).
 * GPIO registers for direct register access without nrfx dependency. */
#define GPIO_P0_BASE        0x50000000u
#define GPIO_DIRSET_OFFSET  0x518u
#define GPIO_OUTSET_OFFSET  0x508u
#define GPIO_OUTCLR_OFFSET  0x50Cu

static inline void fault_led_init(void) {
    volatile uint32_t* dirset = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_DIRSET_OFFSET);
    *dirset = (1u << 26);
}

static inline void fault_led_on(void) {
    volatile uint32_t* outclr = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_OUTCLR_OFFSET);
    *outclr = (1u << 26);  /* active-low: clear = LED on */
}

static inline void fault_led_off(void) {
    volatile uint32_t* outset = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_OUTSET_OFFSET);
    *outset = (1u << 26);  /* active-low: set = LED off */
}

/* Simple spin delay. nRF52840 boots on 16 MHz RC oscillator.
 * Each loop body is ~3 cycles at 16 MHz ≈ ~187 ns/iteration.
 * 80000 iterations ≈ ~15 ms. */
static void spin_delay(volatile uint32_t ticks) {
    while (ticks--) { __asm__ volatile("nop"); }
}

int main(void) {
    struct boot_rsp rsp;
    int rc = boot_go(&rsp);

    if (rc != 0) {
        /* No valid image — blink fault LED forever. */
        fault_led_init();
        while (1) {
            fault_led_on();
            spin_delay(80000u);   /* ~15 ms on */
            fault_led_off();
            spin_delay(80000u);   /* ~15 ms off */
        }
    }

    /* Compute the address of the application's vector table.
     * rsp.br_image_off is the primary slot base (0x00010000).
     * rsp.br_hdr->ih_hdr_size is MCUBOOT_IMAGE_HEADER_SIZE (0x200). */
    uint32_t app_vector_table = rsp.br_image_off + rsp.br_hdr->ih_hdr_size;

    /* Read initial stack pointer and reset vector from the app vector table. */
    uint32_t app_sp = *((uint32_t*)app_vector_table);
    uint32_t app_pc = *((uint32_t*)(app_vector_table + 4u));

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
