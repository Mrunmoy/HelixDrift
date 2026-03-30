#pragma once

#include "Vec3.hpp"

#include <cstddef>
#include <vector>

namespace sim {

struct DipoleSource {
    sf::Vec3 position{};
    sf::Vec3 moment{};
    float decayRadius = 5.0f;

    DipoleSource() = default;
    DipoleSource(const sf::Vec3& pos, const sf::Vec3& mom, float radius = 5.0f)
        : position(pos), moment(mom), decayRadius(radius) {}
};

struct EarthFieldModel {
    float horizontal = 25.0f;
    float vertical = 40.0f;
    float declination = 0.0f;

    EarthFieldModel() = default;
    EarthFieldModel(float h, float v, float d = 0.0f)
        : horizontal(h), vertical(v), declination(d) {}

    sf::Vec3 toVector() const;
};

class MagneticEnvironment {
public:
    struct FieldQuality {
        float totalFieldUt = 0.0f;
        float earthFieldUt = 0.0f;
        float disturbanceUt = 0.0f;
        float disturbanceRatio = 0.0f;
        bool isDisturbed = false;
    };

    MagneticEnvironment();

    void setEarthField(const EarthFieldModel& field);
    EarthFieldModel getEarthField() const { return earthField_; }

    void addDipole(const DipoleSource& dipole);
    void clearDipoles();
    std::size_t getDipoleCount() const { return dipoles_.size(); }

    sf::Vec3 getFieldAt(const sf::Vec3& position) const;
    FieldQuality getQualityAt(const sf::Vec3& position) const;

    static MagneticEnvironment cleanLab();
    static MagneticEnvironment officeDesk();
    static MagneticEnvironment wearableMotion();
    static MagneticEnvironment worstCase();

private:
    sf::Vec3 computeDipoleField(const DipoleSource& dipole, const sf::Vec3& position) const;

    EarthFieldModel earthField_{};
    std::vector<DipoleSource> dipoles_{};

    static constexpr float kMu0Over4Pi = 0.1f;
    static constexpr float kDisturbanceThreshold = 0.2f;
    static constexpr float kSingularityGuardMeters = 1.0e-3f;
};

} // namespace sim
