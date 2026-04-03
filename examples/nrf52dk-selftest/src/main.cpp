#include "BleOtaService.hpp"
#include "IOtaManager.hpp"
#include "NrfOtaFlashBackend.hpp"
#include "OtaTargetIdentity.hpp"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <cstddef>
#include <cstdint>

namespace {
constexpr uint32_t kLedPins[4] = {17u, 18u, 19u, 20u}; // LED1-LED4, active low.
constexpr uint32_t kFlashPageSize = 4096u;
constexpr uint32_t kOtaServicePageBase = 0x0007D000u;
constexpr uint32_t kOtaTestPageBase = 0x0007E000u;
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
    OtaErase = 6,
    OtaWrite = 7,
    OtaVerify = 8,
    OtaServiceBegin = 9,
    OtaServiceWrite = 10,
    OtaServiceCommit = 11,
    Passed = 12,
    Failed = 13,
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

bool runOtaBackendSelfTest() {
    constexpr uint8_t kChunk0[] = {0x41u, 0x42u, 0x43u};
    constexpr uint8_t kChunk1[] = {0x44u, 0x45u, 0x46u, 0x47u, 0x48u};
    constexpr uint8_t kChunk2[] = {0x99u, 0x88u, 0x77u, 0x66u};
    constexpr uint8_t kTailChunk[] = {0x58u, 0x59u, 0x5Au};
    helix::NrfOtaFlashBackend backend{kOtaTestPageBase, kFlashPageSize};

    setPhase(Phase::OtaErase);
    if (!backend.eraseSlot()) {
        g_selfTestStatus.failureCode = 0xE300u;
        return false;
    }

    auto* flash = reinterpret_cast<volatile const uint8_t*>(backend.slotBase());
    for (std::size_t i = 0; i < 16u; ++i) {
        if (flash[i] != 0xFFu) {
            g_selfTestStatus.failureCode = 0xE310u + static_cast<uint32_t>(i);
            return false;
        }
    }

    setPhase(Phase::OtaWrite);
    if (!backend.writeChunk(0u, kChunk0, sizeof(kChunk0))) {
        g_selfTestStatus.failureCode = 0xE320u;
        return false;
    }
    beat();
    if (!backend.writeChunk(3u, kChunk1, sizeof(kChunk1))) {
        g_selfTestStatus.failureCode = 0xE321u;
        return false;
    }
    beat();
    if (!backend.writeChunk(8u, kChunk2, sizeof(kChunk2))) {
        g_selfTestStatus.failureCode = 0xE322u;
        return false;
    }
    beat();
    if (!backend.writeChunk(kFlashPageSize - sizeof(kTailChunk), kTailChunk, sizeof(kTailChunk))) {
        g_selfTestStatus.failureCode = 0xE323u;
        return false;
    }
    beat();
    if (backend.writeChunk(kFlashPageSize - 1u, kTailChunk, sizeof(kTailChunk))) {
        g_selfTestStatus.failureCode = 0xE324u;
        return false;
    }

    setPhase(Phase::OtaVerify);
    constexpr uint8_t kExpectedPrefix[] = {
        0x41u, 0x42u, 0x43u, 0x44u, 0x45u, 0x46u, 0x47u, 0x48u,
        0x99u, 0x88u, 0x77u, 0x66u,
    };
    for (std::size_t i = 0; i < sizeof(kExpectedPrefix); ++i) {
        if (flash[i] != kExpectedPrefix[i]) {
            g_selfTestStatus.failureCode = 0xE330u + static_cast<uint32_t>(i);
            return false;
        }
        g_selfTestStatus.flashVerifiedWords = static_cast<uint32_t>(i + 1u);
    }

    const std::size_t tailBase = kFlashPageSize - sizeof(kTailChunk);
    for (std::size_t i = 0; i < sizeof(kTailChunk); ++i) {
        if (flash[tailBase + i] != kTailChunk[i]) {
            g_selfTestStatus.failureCode = 0xE340u + static_cast<uint32_t>(i);
            return false;
        }
    }

    return true;
}

uint32_t crc32(const uint8_t* data, std::size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? ((crc >> 1u) ^ 0xEDB88320u) : (crc >> 1u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

bool runOtaServiceSelfTest() {
    constexpr uint8_t kImage[] = {0x48u, 0x45u, 0x4Cu, 0x49u, 0x58u, 0x44u, 0x4Bu, 0x21u, 0x0Au};
    helix::NrfOtaFlashBackend backend{kOtaServicePageBase, kFlashPageSize};
    helix::OtaManager manager{backend};
    helix::OtaManagerAdapter adapter{manager};
    helix::BleOtaService service{adapter, helix::kOtaTargetIdNrf52dkNrf52832};

    const uint32_t crc = crc32(kImage, sizeof(kImage));
    uint8_t beginPacket[13] = {
        helix::BleOtaService::CMD_BEGIN,
        static_cast<uint8_t>(sizeof(kImage) & 0xFFu),
        static_cast<uint8_t>((sizeof(kImage) >> 8u) & 0xFFu),
        static_cast<uint8_t>((sizeof(kImage) >> 16u) & 0xFFu),
        static_cast<uint8_t>((sizeof(kImage) >> 24u) & 0xFFu),
        static_cast<uint8_t>(crc & 0xFFu),
        static_cast<uint8_t>((crc >> 8u) & 0xFFu),
        static_cast<uint8_t>((crc >> 16u) & 0xFFu),
        static_cast<uint8_t>((crc >> 24u) & 0xFFu),
        static_cast<uint8_t>(helix::kOtaTargetIdNrf52dkNrf52832 & 0xFFu),
        static_cast<uint8_t>((helix::kOtaTargetIdNrf52dkNrf52832 >> 8u) & 0xFFu),
        static_cast<uint8_t>((helix::kOtaTargetIdNrf52dkNrf52832 >> 16u) & 0xFFu),
        static_cast<uint8_t>((helix::kOtaTargetIdNrf52dkNrf52832 >> 24u) & 0xFFu),
    };

    setPhase(Phase::OtaServiceBegin);
    if (service.handleControlWrite(beginPacket, sizeof(beginPacket)) != helix::OtaStatus::OK) {
        g_selfTestStatus.failureCode = 0xE400u;
        return false;
    }

    uint8_t chunk0[7] = {0u, 0u, 0u, 0u, kImage[0], kImage[1], kImage[2]};
    setPhase(Phase::OtaServiceWrite);
    if (service.handleDataWrite(chunk0, sizeof(chunk0)) != helix::OtaStatus::OK) {
        g_selfTestStatus.failureCode = 0xE410u;
        return false;
    }
    beat();

    uint8_t chunk1[10] = {3u, 0u, 0u, 0u, kImage[3], kImage[4], kImage[5], kImage[6], kImage[7], kImage[8]};
    if (service.handleDataWrite(chunk1, sizeof(chunk1)) != helix::OtaStatus::OK) {
        g_selfTestStatus.failureCode = 0xE411u;
        return false;
    }
    beat();

    setPhase(Phase::OtaServiceCommit);
    const uint8_t commitCmd = helix::BleOtaService::CMD_COMMIT;
    if (service.handleControlWrite(&commitCmd, 1u) != helix::OtaStatus::OK) {
        g_selfTestStatus.failureCode = 0xE420u;
        return false;
    }

    uint8_t status[helix::BleOtaService::kStatusLen] = {};
    size_t statusLen = 0u;
    service.getStatus(status, &statusLen);
    if (statusLen != helix::BleOtaService::kStatusLen) {
        g_selfTestStatus.failureCode = 0xE430u;
        return false;
    }
    if (status[0] != static_cast<uint8_t>(helix::OtaState::COMMITTED) ||
        status[1] != sizeof(kImage) ||
        status[5] != static_cast<uint8_t>(helix::OtaStatus::OK) ||
        status[6] != static_cast<uint8_t>(helix::kOtaTargetIdNrf52dkNrf52832 & 0xFFu) ||
        status[7] != static_cast<uint8_t>((helix::kOtaTargetIdNrf52dkNrf52832 >> 8u) & 0xFFu) ||
        status[8] != static_cast<uint8_t>((helix::kOtaTargetIdNrf52dkNrf52832 >> 16u) & 0xFFu) ||
        status[9] != static_cast<uint8_t>((helix::kOtaTargetIdNrf52dkNrf52832 >> 24u) & 0xFFu)) {
        g_selfTestStatus.failureCode = 0xE431u;
        return false;
    }

    auto* flash = reinterpret_cast<volatile const uint8_t*>(backend.slotBase());
    for (std::size_t i = 0; i < sizeof(kImage); ++i) {
        if (flash[i] != kImage[i]) {
            g_selfTestStatus.failureCode = 0xE440u + static_cast<uint32_t>(i);
            return false;
        }
    }

    return true;
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
    if (!runOtaBackendSelfTest()) {
        signalFailure();
    }
    if (!runOtaServiceSelfTest()) {
        signalFailure();
    }
    signalSuccess();
}
