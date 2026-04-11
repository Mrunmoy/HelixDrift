#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <cstdint>

namespace {

constexpr uint32_t kLedPin = 15;   // Candidate user LED on ProPico / SuperMini family.
constexpr uint32_t kStatusMagic = 0x50505250u; // "PPRP"

enum class Phase : uint32_t {
    Boot = 0,
    HeartbeatOn = 1,
    HeartbeatOff = 2,
    Delay = 3,
};

struct BringupStatus {
    uint32_t magic;
    uint32_t phase;
    uint32_t heartbeatCount;
    uint32_t ledPin;
    uint32_t outRegister;
    uint32_t inRegister;
};

volatile BringupStatus g_bringupStatus __attribute__((section(".noinit.bringup")));

void setPhase(Phase phase) {
    g_bringupStatus.phase = static_cast<uint32_t>(phase);
}

void ledSet(bool on) {
    if (on) {
        nrf_gpio_pin_set(kLedPin);
    } else {
        nrf_gpio_pin_clear(kLedPin);
    }
    g_bringupStatus.outRegister = nrf_gpio_port0()->OUT;
    g_bringupStatus.inRegister = nrf_gpio_port0()->IN;
}

} // namespace

int main() {
    g_bringupStatus.magic = kStatusMagic;
    g_bringupStatus.phase = static_cast<uint32_t>(Phase::Boot);
    g_bringupStatus.heartbeatCount = 0u;
    g_bringupStatus.ledPin = kLedPin;
    g_bringupStatus.outRegister = 0u;
    g_bringupStatus.inRegister = 0u;

    nrf_gpio_cfg_output(kLedPin);
    ledSet(false);

    while (true) {
        setPhase(Phase::HeartbeatOn);
        ledSet(true);
        g_bringupStatus.heartbeatCount += 1u;
        setPhase(Phase::Delay);
        nrf_delay_ms(250u);

        setPhase(Phase::HeartbeatOff);
        ledSet(false);
        setPhase(Phase::Delay);
        nrf_delay_ms(250u);
    }
}
