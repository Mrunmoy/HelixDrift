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
#define GPIO_OUTCLR_OFFSET  0x50Cu

static inline void fault_led_init(void) {
    volatile uint32_t* dirset = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_DIRSET_OFFSET);
    *dirset = (1u << 26);
}

static inline void fault_led_on(void) {
    volatile uint32_t* outclr = (volatile uint32_t*)(GPIO_P0_BASE + GPIO_OUTCLR_OFFSET);
    *outclr = (1u << 26);
}

/* Simple spin delay (rough, calibrated for 64 MHz system clock). */
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
            spin_delay(3200000u);  /* ~50 ms on at 64 MHz */
        }
    }

    /* Compute the address of the application's vector table.
     * rsp.br_image_off is the primary slot base (0x00010000).
     * rsp.br_hdr->ih_hdr_size is MCUBOOT_IMAGE_HEADER_SIZE (0x200). */
    uint32_t app_vector_table = rsp.br_image_off + rsp.br_hdr->ih_hdr_size;

    /* Read initial stack pointer and reset vector from the app vector table. */
    uint32_t app_sp = *((uint32_t*)app_vector_table);
    uint32_t app_pc = *((uint32_t*)(app_vector_table + 4u));

    /* Relocate VTOR to the application's vector table. */
    SCB_VTOR = app_vector_table;

    /* Set the main stack pointer and jump to the application. */
    __asm__ volatile(
        "msr msp, %0  \n"
        "bx  %1       \n"
        :
        : "r"(app_sp), "r"(app_pc)
        :
    );

    /* Unreachable */
    while (1) {}
    return 0;
}
