#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <cstddef>
#include <cstdint>

namespace {
constexpr uint32_t kLedPins[4] = {17u, 18u, 19u, 20u}; // LED1-LED4, active low.
constexpr uint32_t kFlashPageSize = 4096u;
constexpr uint32_t kTestPageBase = 0x0007F000u; // Last page of DK NVS region.
constexpr uint32_t kStatusMagic = 0x48445837u;   // "HDX7"

constexpr uintptr_t kNvmcBase = 0x4001E000u;
constexpr uintptr_t kNvmcReadyOffset = 0x400u;
constexpr uintptr_t kNvmcConfigOffset = 0x504u;
constexpr uintptr_t kNvmcErasePageOffset = 0x508u;
constexpr std::size_t kPatternWordCount = 4u;

enum class Phase : uint32_t {
    Boot = 0,
    LedSweep = 1,
    FlashErase = 2,
    FlashWrite = 3,
    FlashVerify = 4,
    FlashRestore = 5,
    Passed = 6,
    Failed = 7,
};

struct SelfTestStatus {
    uint32_t magic;
    uint32_t phase;
    uint32_t heartbeat;
    uint32_t ledSweepCount;
    uint32_t flashVerifiedWords;
    uint32_t failureCode;
};

volatile SelfTestStatus g_selfTestStatus __attribute__((section(".noinit.selftest")));

volatile uint32_t& reg32(uintptr_t addr) {
    return *reinterpret_cast<volatile uint32_t*>(addr);
}

void setPhase(Phase phase) {
    g_selfTestStatus.phase = static_cast<uint32_t>(phase);
}

void beat() {
    ++g_selfTestStatus.heartbeat;
}

void ledWrite(std::size_t index, bool on) {
    if (index >= 4) {
        return;
    }
    if (on) {
        nrf_gpio_pin_clear(kLedPins[index]);
    } else {
        nrf_gpio_pin_set(kLedPins[index]);
    }
}

void allLedsOff() {
    for (std::size_t i = 0; i < 4; ++i) {
        ledWrite(i, false);
    }
}

void initLeds() {
    for (uint32_t pin : kLedPins) {
        nrf_gpio_cfg_output(pin);
    }
    allLedsOff();
}

void waitReady() {
    while (reg32(kNvmcBase + kNvmcReadyOffset) == 0u) {
    }
}

void nvmcSetMode(uint32_t mode) {
    reg32(kNvmcBase + kNvmcConfigOffset) = mode;
    waitReady();
}

void erasePage(uint32_t pageAddr) {
    nvmcSetMode(2u);
    reg32(kNvmcBase + kNvmcErasePageOffset) = pageAddr;
    waitReady();
    nvmcSetMode(0u);
}

void writeWord(uint32_t addr, uint32_t value) {
    nvmcSetMode(1u);
    *reinterpret_cast<volatile uint32_t*>(addr) = value;
    waitReady();
    nvmcSetMode(0u);
}

bool isErased(uint32_t addr, std::size_t words) {
    auto* flash = reinterpret_cast<volatile const uint32_t*>(addr);
    for (std::size_t i = 0; i < words; ++i) {
        if (flash[i] != 0xFFFFFFFFu) {
            g_selfTestStatus.failureCode = 0xE100u + static_cast<uint32_t>(i);
            return false;
        }
    }
    return true;
}

bool verifyPattern(uint32_t addr, const uint32_t* pattern, std::size_t words) {
    auto* flash = reinterpret_cast<volatile const uint32_t*>(addr);
    for (std::size_t i = 0; i < words; ++i) {
        if (flash[i] != pattern[i]) {
            g_selfTestStatus.failureCode = 0xE200u + static_cast<uint32_t>(i);
            return false;
        }
        g_selfTestStatus.flashVerifiedWords = static_cast<uint32_t>(i + 1u);
    }
    return true;
}

void signalFailure() {
    setPhase(Phase::Failed);
    allLedsOff();
    while (true) {
        beat();
        ledWrite(3, true);
        nrf_delay_ms(120);
        ledWrite(3, false);
        nrf_delay_ms(120);
    }
}

void runLedSweep() {
    setPhase(Phase::LedSweep);
    for (std::size_t pass = 0; pass < 3; ++pass) {
        for (std::size_t i = 0; i < 4; ++i) {
            allLedsOff();
            ledWrite(i, true);
            beat();
            nrf_delay_ms(150);
        }
        ++g_selfTestStatus.ledSweepCount;
    }
    allLedsOff();
}

bool runFlashSelfTest() {
    constexpr uint32_t kInitialPattern[kPatternWordCount] = {
        0x12345678u,
        0xA5A55A5Au,
        0xCAFEBABEu,
        0x0BADF00Du,
    };
    constexpr uint32_t kSuccessPattern[kPatternWordCount] = {
        0x48444B31u, // "HDK1"
        0x4F4B4159u, // "OKAY"
        0x00000004u, // four verified words
        0x0007F000u, // test page base
    };

    setPhase(Phase::FlashErase);
    erasePage(kTestPageBase);
    if (!isErased(kTestPageBase, kFlashPageSize / sizeof(uint32_t))) {
        return false;
    }

    setPhase(Phase::FlashWrite);
    for (std::size_t i = 0; i < kPatternWordCount; ++i) {
        writeWord(kTestPageBase + static_cast<uint32_t>(i * sizeof(uint32_t)), kInitialPattern[i]);
        beat();
    }

    setPhase(Phase::FlashVerify);
    if (!verifyPattern(kTestPageBase, kInitialPattern, kPatternWordCount)) {
        return false;
    }

    setPhase(Phase::FlashRestore);
    erasePage(kTestPageBase);
    if (!isErased(kTestPageBase, kFlashPageSize / sizeof(uint32_t))) {
        return false;
    }

    setPhase(Phase::FlashWrite);
    for (std::size_t i = 0; i < kPatternWordCount; ++i) {
        writeWord(kTestPageBase + static_cast<uint32_t>(i * sizeof(uint32_t)), kSuccessPattern[i]);
        beat();
    }

    setPhase(Phase::FlashVerify);
    return verifyPattern(kTestPageBase, kSuccessPattern, kPatternWordCount);
}

void signalSuccess() {
    setPhase(Phase::Passed);
    allLedsOff();
    while (true) {
        beat();
        ledWrite(0, true);
        ledWrite(1, false);
        nrf_delay_ms(250);
        ledWrite(0, false);
        ledWrite(1, true);
        nrf_delay_ms(250);
    }
}
} // namespace

int main() {
    g_selfTestStatus.magic = kStatusMagic;
    g_selfTestStatus.phase = static_cast<uint32_t>(Phase::Boot);
    g_selfTestStatus.heartbeat = 0u;
    g_selfTestStatus.ledSweepCount = 0u;
    g_selfTestStatus.flashVerifiedWords = 0u;
    g_selfTestStatus.failureCode = 0u;

    initLeds();
    runLedSweep();
    if (!runFlashSelfTest()) {
        signalFailure();
    }
    signalSuccess();
}
