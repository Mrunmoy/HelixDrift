#include "BlinkEngine.hpp"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <cstdint>

namespace {
constexpr uint32_t kLedPin = 13;
constexpr uint32_t kTickMs = 10;
}

int main() {
    nrf_gpio_cfg_output(kLedPin);

    helix::BlinkEngine blink(500, 500, true);
    bool level = blink.level();
    if (level) nrf_gpio_pin_set(kLedPin);
    else nrf_gpio_pin_clear(kLedPin);

    while (true) {
        const bool newLevel = blink.tick(kTickMs);
        if (newLevel != level) {
            level = newLevel;
            if (level) nrf_gpio_pin_set(kLedPin);
            else nrf_gpio_pin_clear(kLedPin);
        }
        nrf_delay_ms(kTickMs);
    }
}
