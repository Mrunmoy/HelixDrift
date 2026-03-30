#include "HardIronCalibrator.hpp"

#include <algorithm>
#include <cmath>

namespace sim {

namespace {

sf::Vec3 midpoint(const sf::Vec3& a, const sf::Vec3& b) {
    return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f};
}

float distance(const sf::Vec3& a, const sf::Vec3& b) {
    return (a - b).length();
}

} // namespace

void HardIronCalibrator::startCalibration() {
    samples_.clear();
}

void HardIronCalibrator::addSample(const sf::Vec3& sampleUt) {
    samples_.push_back(sampleUt);
}

sf::Vec3 HardIronCalibrator::getOffset() const {
    if (samples_.empty()) {
        return {};
    }

    sf::Vec3 min = samples_.front();
    sf::Vec3 max = samples_.front();
    for (const sf::Vec3& sample : samples_) {
        min.x = std::min(min.x, sample.x);
        min.y = std::min(min.y, sample.y);
        min.z = std::min(min.z, sample.z);
        max.x = std::max(max.x, sample.x);
        max.y = std::max(max.y, sample.y);
        max.z = std::max(max.z, sample.z);
    }
    return midpoint(min, max);
}

float HardIronCalibrator::getRadiusUt() const {
    if (samples_.empty()) {
        return 0.0f;
    }

    const sf::Vec3 offset = getOffset();
    float sum = 0.0f;
    for (const sf::Vec3& sample : samples_) {
        sum += distance(sample, offset);
    }
    return sum / static_cast<float>(samples_.size());
}

float HardIronCalibrator::getConfidence() const {
    if (samples_.size() < 6u) {
        return 0.0f;
    }

    sf::Vec3 min = samples_.front();
    sf::Vec3 max = samples_.front();
    for (const sf::Vec3& sample : samples_) {
        min.x = std::min(min.x, sample.x);
        min.y = std::min(min.y, sample.y);
        min.z = std::min(min.z, sample.z);
        max.x = std::max(max.x, sample.x);
        max.y = std::max(max.y, sample.y);
        max.z = std::max(max.z, sample.z);
    }

    const float radius = getRadiusUt();
    if (radius <= 1.0e-3f) {
        return 0.0f;
    }

    const float idealSpan = 2.0f * radius;
    const float spanX = std::clamp((max.x - min.x) / idealSpan, 0.0f, 1.0f);
    const float spanY = std::clamp((max.y - min.y) / idealSpan, 0.0f, 1.0f);
    const float spanZ = std::clamp((max.z - min.z) / idealSpan, 0.0f, 1.0f);
    const float coverageScore = std::min({spanX, spanY, spanZ});
    const float sampleScore = std::clamp(static_cast<float>(samples_.size()) / 6.0f, 0.0f, 1.0f);
    return coverageScore * sampleScore;
}

bool HardIronCalibrator::hasSolution() const {
    return samples_.size() >= 6u && getConfidence() >= 0.4f;
}

} // namespace sim
