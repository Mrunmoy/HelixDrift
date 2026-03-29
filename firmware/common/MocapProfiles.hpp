#pragma once

#include <cstdint>

namespace helix {

enum class MocapPowerMode : uint8_t {
    PERFORMANCE = 0,
    BATTERY = 1,
};

struct MocapProfile {
    uint32_t outputPeriodUs;
    float dtSeconds;
    uint16_t imuOdrHz;
    uint16_t magOdrHz;
    uint16_t baroOdrHz;
};

constexpr MocapProfile kPerformanceProfile{
    20000,      // 50 Hz output
    1.0f / 50.0f,
    208,
    100,
    200,
};

constexpr MocapProfile kBatteryProfile{
    25000,      // 40 Hz output
    1.0f / 40.0f,
    104,
    50,
    25,
};

constexpr MocapProfile selectMocapProfile(MocapPowerMode mode) {
    return (mode == MocapPowerMode::PERFORMANCE) ? kPerformanceProfile : kBatteryProfile;
}

} // namespace helix
