#pragma once

#include "Quaternion.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sim {

struct ErrorSeriesStats {
    float meanDeg = 0.0f;
    float rmsDeg = 0.0f;
    float maxDeg = 0.0f;
};

inline float angularErrorDeg(const sf::Quaternion& truth, const sf::Quaternion& fused) {
    float dot = std::abs(truth.w * fused.w +
                         truth.x * fused.x +
                         truth.y * fused.y +
                         truth.z * fused.z);
    dot = std::clamp(dot, 0.0f, 1.0f);
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    return 2.0f * std::acos(dot) * kRadToDeg;
}

inline ErrorSeriesStats summarizeErrorSeriesDeg(const std::vector<float>& errorsDeg) {
    ErrorSeriesStats stats{};
    if (errorsDeg.empty()) {
        return stats;
    }

    float errorSum = 0.0f;
    float squaredErrorSum = 0.0f;
    float maxError = 0.0f;
    for (float errorDeg : errorsDeg) {
        errorSum += errorDeg;
        squaredErrorSum += errorDeg * errorDeg;
        if (errorDeg > maxError) {
            maxError = errorDeg;
        }
    }

    stats.meanDeg = errorSum / static_cast<float>(errorsDeg.size());
    stats.rmsDeg = std::sqrt(squaredErrorSum / static_cast<float>(errorsDeg.size()));
    stats.maxDeg = maxError;
    return stats;
}

inline int firstIndexAtOrBelowDeg(const std::vector<float>& errorsDeg, float thresholdDeg) {
    for (std::size_t i = 0; i < errorsDeg.size(); ++i) {
        if (errorsDeg[i] <= thresholdDeg) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline float linearDriftRateDegPerMin(const std::vector<float>& errorsDeg,
                                      uint64_t samplePeriodUs,
                                      float trailingFraction = 0.5f) {
    if (errorsDeg.size() < 2u || samplePeriodUs == 0u) {
        return 0.0f;
    }

    trailingFraction = std::clamp(trailingFraction, 0.0f, 1.0f);
    std::size_t startIndex = 0u;
    if (trailingFraction > 0.0f) {
        const std::size_t trailingCount = std::max<std::size_t>(
            2u, static_cast<std::size_t>(std::ceil(errorsDeg.size() * trailingFraction)));
        startIndex = errorsDeg.size() - trailingCount;
    }

    const std::size_t count = errorsDeg.size() - startIndex;
    if (count < 2u) {
        return 0.0f;
    }

    const double dtMinutes = static_cast<double>(samplePeriodUs) / (1000000.0 * 60.0);
    double sumT = 0.0;
    double sumE = 0.0;
    double sumTT = 0.0;
    double sumTE = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) * dtMinutes;
        const double e = static_cast<double>(errorsDeg[startIndex + i]);
        sumT += t;
        sumE += e;
        sumTT += t * t;
        sumTE += t * e;
    }

    const double denom = static_cast<double>(count) * sumTT - sumT * sumT;
    if (std::abs(denom) < 1e-12) {
        return 0.0f;
    }

    const double slope = (static_cast<double>(count) * sumTE - sumT * sumE) / denom;
    return static_cast<float>(slope);
}

} // namespace sim
