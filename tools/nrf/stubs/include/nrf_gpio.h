#pragma once

#include <stdint.h>

static inline void nrf_gpio_cfg_output(uint32_t pin) { (void)pin; }
static inline void nrf_gpio_pin_set(uint32_t pin) { (void)pin; }
static inline void nrf_gpio_pin_clear(uint32_t pin) { (void)pin; }
static inline void nrf_gpio_pin_toggle(uint32_t pin) { (void)pin; }
