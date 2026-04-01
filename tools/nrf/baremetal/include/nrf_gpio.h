#pragma once

#include <stdint.h>

enum {
    NRF_GPIO_PIN_CNF_DIR_Pos = 0,
    NRF_GPIO_PIN_CNF_INPUT_Pos = 1,
    NRF_GPIO_PIN_CNF_PULL_Pos = 2,
    NRF_GPIO_PIN_CNF_DRIVE_Pos = 8,
    NRF_GPIO_PIN_CNF_SENSE_Pos = 16,
};

typedef struct {
    volatile uint32_t OUT;
    volatile uint32_t OUTSET;
    volatile uint32_t OUTCLR;
    volatile uint32_t IN;
    volatile uint32_t DIR;
    volatile uint32_t DIRSET;
    volatile uint32_t DIRCLR;
    uint32_t _reserved0[120];
    volatile uint32_t PIN_CNF[32];
} nrf_gpio_regs_t;

static inline nrf_gpio_regs_t* nrf_gpio_port0(void) {
    return (nrf_gpio_regs_t*)0x50000000u;
}

static inline void nrf_gpio_cfg_output(uint32_t pin) {
    nrf_gpio_regs_t* gpio = nrf_gpio_port0();
    gpio->PIN_CNF[pin] =
        (1u << NRF_GPIO_PIN_CNF_DIR_Pos) |
        (1u << NRF_GPIO_PIN_CNF_INPUT_Pos) |
        (0u << NRF_GPIO_PIN_CNF_PULL_Pos) |
        (0u << NRF_GPIO_PIN_CNF_DRIVE_Pos) |
        (0u << NRF_GPIO_PIN_CNF_SENSE_Pos);
    gpio->DIRSET = (1u << pin);
}

static inline void nrf_gpio_cfg_input(uint32_t pin, uint32_t pull_config) {
    nrf_gpio_regs_t* gpio = nrf_gpio_port0();
    gpio->PIN_CNF[pin] =
        (0u << NRF_GPIO_PIN_CNF_DIR_Pos) |
        (0u << NRF_GPIO_PIN_CNF_INPUT_Pos) |
        ((pull_config & 0x3u) << NRF_GPIO_PIN_CNF_PULL_Pos) |
        (0u << NRF_GPIO_PIN_CNF_DRIVE_Pos) |
        (0u << NRF_GPIO_PIN_CNF_SENSE_Pos);
    gpio->DIRCLR = (1u << pin);
}

static inline void nrf_gpio_pin_set(uint32_t pin) {
    nrf_gpio_port0()->OUTSET = (1u << pin);
}

static inline void nrf_gpio_pin_clear(uint32_t pin) {
    nrf_gpio_port0()->OUTCLR = (1u << pin);
}

static inline void nrf_gpio_pin_toggle(uint32_t pin) {
    nrf_gpio_regs_t* gpio = nrf_gpio_port0();
    if ((gpio->OUT & (1u << pin)) != 0u) {
        gpio->OUTCLR = (1u << pin);
    } else {
        gpio->OUTSET = (1u << pin);
    }
}

static inline uint32_t nrf_gpio_pin_read(uint32_t pin) {
    return (nrf_gpio_port0()->IN >> pin) & 1u;
}
