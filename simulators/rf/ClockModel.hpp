#pragma once

#include <cmath>
#include <cstdint>
#include <random>

namespace sim {

struct ClockModel {
    float driftPpm = 0.0f;
    uint64_t offsetUs = 0;
    float jitterUs = 0.0f;

    uint64_t mapTrueToLocalUs(uint64_t trueTimeUs) const {
        const long double scale = 1.0L + static_cast<long double>(driftPpm) / 1000000.0L;
        const long double local = static_cast<long double>(offsetUs) +
                                  static_cast<long double>(trueTimeUs) * scale;
        return static_cast<uint64_t>(std::llround(local));
    }

    int64_t offsetAtTrueTimeUs(uint64_t trueTimeUs) const {
        return static_cast<int64_t>(mapTrueToLocalUs(trueTimeUs)) -
               static_cast<int64_t>(trueTimeUs);
    }

    static ClockModel randomCrystal(float maxDriftPpm = 20.0f) {
        static std::mt19937 rng(42u);
        std::uniform_real_distribution<float> drift(-maxDriftPpm, maxDriftPpm);
        std::uniform_int_distribution<uint64_t> offset(0u, 10000u);
        return ClockModel{drift(rng), offset(rng), 0.0f};
    }

    static ClockModel randomTCXO(float maxDriftPpm = 2.0f) {
        static std::mt19937 rng(43u);
        std::uniform_real_distribution<float> drift(-maxDriftPpm, maxDriftPpm);
        std::uniform_int_distribution<uint64_t> offset(0u, 2000u);
        return ClockModel{drift(rng), offset(rng), 0.0f};
    }
};

} // namespace sim
