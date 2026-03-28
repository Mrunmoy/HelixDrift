#pragma once

#include "Quaternion.hpp"

#include <algorithm>
#include <cmath>

namespace sim {

inline float angularErrorDeg(const sf::Quaternion& truth, const sf::Quaternion& fused) {
    float dot = std::abs(truth.w * fused.w +
                         truth.x * fused.x +
                         truth.y * fused.y +
                         truth.z * fused.z);
    dot = std::clamp(dot, 0.0f, 1.0f);
    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    return 2.0f * std::acos(dot) * kRadToDeg;
}

} // namespace sim
