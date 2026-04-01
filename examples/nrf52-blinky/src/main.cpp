#include "BlinkEngine.hpp"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <cstdint>

namespace {
#ifndef HELIX_LED_PIN
#define HELIX_LED_PIN 13
#endif

#ifndef HELIX_LED_ACTIVE_LOW
#define HELIX_LED_ACTIVE_LOW 0
#endif

constexpr uint32_t kLedPin = HELIX_LED_PIN;
constexpr uint32_t kTickMs = 10;

void driveLed(bool on) {
#if HELIX_LED_ACTIVE_LOW
    if (on) nrf_gpio_pin_clear(kLedPin);
    else nrf_gpio_pin_set(kLedPin);
#else
    if (on) nrf_gpio_pin_set(kLedPin);
    else nrf_gpio_pin_clear(kLedPin);
#endif
}
}

int main() {
    nrf_gpio_cfg_output(kLedPin);

    helix::BlinkEngine blink(500, 500, true);
    bool level = blink.level();
    driveLed(level);

    while (true) {
        const bool newLevel = blink.tick(kTickMs);
        if (newLevel != level) {
            level = newLevel;
            driveLed(level);
        }
        nrf_delay_ms(kTickMs);
    }
}
