#include "MagneticEnvironment.hpp"

#include <algorithm>
#include <cmath>

namespace sim {

namespace {

sf::Vec3 scaleVector(const sf::Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

} // namespace

sf::Vec3 EarthFieldModel::toVector() const {
    const float declinationRad = declination * static_cast<float>(M_PI) / 180.0f;
    const float north = horizontal * std::cos(declinationRad);
    const float east = horizontal * std::sin(declinationRad);
    return {north, east, vertical};
}

MagneticEnvironment::MagneticEnvironment() = default;

void MagneticEnvironment::setEarthField(const EarthFieldModel& field) {
    earthField_ = field;
}

void MagneticEnvironment::addDipole(const DipoleSource& dipole) {
    dipoles_.push_back(dipole);
}

void MagneticEnvironment::clearDipoles() {
    dipoles_.clear();
}

sf::Vec3 MagneticEnvironment::getFieldAt(const sf::Vec3& position) const {
    sf::Vec3 totalField = earthField_.toVector();
    for (const DipoleSource& dipole : dipoles_) {
        const sf::Vec3 r = position - dipole.position;
        if (r.length() > dipole.decayRadius) {
            continue;
        }
        totalField = totalField + computeDipoleField(dipole, position);
    }
    return totalField;
}

MagneticEnvironment::FieldQuality MagneticEnvironment::getQualityAt(const sf::Vec3& position) const {
    const sf::Vec3 earthField = earthField_.toVector();
    const sf::Vec3 totalField = getFieldAt(position);
    const sf::Vec3 disturbance = totalField - earthField;

    const float earthMagnitude = earthField.length();
    const float disturbanceMagnitude = disturbance.length();
    const float ratio = earthMagnitude > 0.0f ? disturbanceMagnitude / earthMagnitude : 0.0f;

    return {
        totalField.length(),
        earthMagnitude,
        disturbanceMagnitude,
        ratio,
        ratio > kDisturbanceThreshold,
    };
}

MagneticEnvironment MagneticEnvironment::cleanLab() {
    MagneticEnvironment env;
    env.setEarthField({25.0f, 40.0f, 0.0f});
    return env;
}

MagneticEnvironment MagneticEnvironment::officeDesk() {
    MagneticEnvironment env = cleanLab();
    env.addDipole({{0.25f, 0.10f, 0.00f}, {0.0f, 0.0f, 80.0f}, 2.0f});
    env.addDipole({{-0.35f, 0.20f, 0.10f}, {15.0f, 0.0f, 20.0f}, 1.5f});
    return env;
}

MagneticEnvironment MagneticEnvironment::wearableMotion() {
    MagneticEnvironment env = cleanLab();
    env.addDipole({{0.05f, 0.00f, 0.02f}, {0.0f, 18.0f, 10.0f}, 0.5f});
    env.addDipole({{-0.08f, 0.03f, -0.02f}, {10.0f, -6.0f, 8.0f}, 0.5f});
    return env;
}

MagneticEnvironment MagneticEnvironment::worstCase() {
    MagneticEnvironment env = cleanLab();
    env.addDipole({{0.10f, 0.00f, 0.00f}, {0.0f, 0.0f, 250.0f}, 1.0f});
    env.addDipole({{-0.12f, 0.05f, 0.02f}, {-120.0f, 40.0f, 30.0f}, 1.0f});
    env.addDipole({{0.00f, -0.10f, 0.08f}, {50.0f, -90.0f, 70.0f}, 1.0f});
    return env;
}

sf::Vec3 MagneticEnvironment::computeDipoleField(const DipoleSource& dipole, const sf::Vec3& position) const {
    const sf::Vec3 r = position - dipole.position;
    const float rMagnitude = std::max(r.length(), kSingularityGuardMeters);
    const float invR = 1.0f / rMagnitude;
    const sf::Vec3 rHat = scaleVector(r, invR);
    const float mDotRHat = dipole.moment.dot(rHat);
    const sf::Vec3 term =
        scaleVector(rHat, 3.0f * mDotRHat) - dipole.moment;
    const float scale = kMu0Over4Pi / (rMagnitude * rMagnitude * rMagnitude);
    return scaleVector(term, scale);
}

} // namespace sim
