#pragma once

#include <stdint.h>

enum {
    HELIX_SYSTICK_CTRL_ENABLE_Pos = 0,
    HELIX_SYSTICK_CTRL_CLKSOURCE_Pos = 2,
    HELIX_SYSTICK_CTRL_COUNTFLAG_Pos = 16,
};

typedef struct {
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} helix_systick_regs_t;

static inline helix_systick_regs_t* helix_systick(void) {
    return (helix_systick_regs_t*)0xE000E010u;
}

static inline void nrf_delay_us(uint32_t us) {
    helix_systick_regs_t* const systick = helix_systick();
    while (us-- > 0u) {
        systick->CTRL = 0u;
        systick->LOAD = 64u - 1u; // 1 us at 64 MHz.
        systick->VAL = 0u;
        systick->CTRL =
            (1u << HELIX_SYSTICK_CTRL_ENABLE_Pos) |
            (1u << HELIX_SYSTICK_CTRL_CLKSOURCE_Pos);
        while ((systick->CTRL & (1u << HELIX_SYSTICK_CTRL_COUNTFLAG_Pos)) == 0u) {
            __asm volatile("" ::: "memory");
        }
    }
    systick->CTRL = 0u;
}

static inline void nrf_delay_ms(uint32_t ms) {
    while (ms-- > 0u) {
        nrf_delay_us(1000u);
    }
}
