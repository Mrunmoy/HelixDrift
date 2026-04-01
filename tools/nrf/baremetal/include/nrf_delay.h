#pragma once

#include <stdint.h>

static inline void nrf_delay_us(uint32_t us) {
    volatile uint32_t iterations = us * 16u;
    while (iterations-- > 0u) {
        __asm volatile("nop");
    }
}

static inline void nrf_delay_ms(uint32_t ms) {
    while (ms-- > 0u) {
        nrf_delay_us(1000u);
    }
}
