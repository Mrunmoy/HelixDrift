#pragma once

#include "Vec3.hpp"

#include <cstddef>
#include <vector>

namespace sim {

class HardIronCalibrator {
public:
    void startCalibration();
    void addSample(const sf::Vec3& sampleUt);

    std::size_t sampleCount() const { return samples_.size(); }
    sf::Vec3 getOffset() const;
    float getRadiusUt() const;
    float getConfidence() const;
    bool hasSolution() const;

private:
    std::vector<sf::Vec3> samples_{};
};

} // namespace sim
