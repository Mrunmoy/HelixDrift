#include "MagneticEnvironment.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace sim {
namespace {

float magnitude(const sf::Vec3& v) {
    return std::sqrt(v.lengthSq());
}

} // namespace

TEST(MagneticEnvironment, EarthFieldOnlyUniformEverywhere) {
    MagneticEnvironment env;
    env.setEarthField({25.0f, 40.0f, 0.0f});

    const sf::Vec3 atOrigin = env.getFieldAt({0.0f, 0.0f, 0.0f});
    const sf::Vec3 atOffset = env.getFieldAt({10.0f, -3.0f, 2.5f});

    EXPECT_NEAR(atOrigin.x, atOffset.x, 1.0e-4f);
    EXPECT_NEAR(atOrigin.y, atOffset.y, 1.0e-4f);
    EXPECT_NEAR(atOrigin.z, atOffset.z, 1.0e-4f);
    EXPECT_NEAR(magnitude(atOrigin), std::sqrt(25.0f * 25.0f + 40.0f * 40.0f), 1.0e-3f);
}

TEST(MagneticEnvironment, DipoleFieldDecaysWithDistance) {
    MagneticEnvironment env;
    env.setEarthField({0.0f, 0.0f, 0.0f});
    env.addDipole({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}});

    const sf::Vec3 nearField = env.getFieldAt({0.2f, 0.0f, 0.0f});
    const sf::Vec3 farField = env.getFieldAt({0.4f, 0.0f, 0.0f});

    EXPECT_GT(magnitude(nearField), magnitude(farField) * 6.0f);
}

TEST(MagneticEnvironment, FieldSuperpositionIsLinear) {
    MagneticEnvironment combined;
    combined.setEarthField({0.0f, 0.0f, 0.0f});
    combined.addDipole({{1.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}});
    combined.addDipole({{0.0f, 1.0f, 0.0f}, {0.0f, 10.0f, 0.0f}});

    MagneticEnvironment envA;
    envA.setEarthField({0.0f, 0.0f, 0.0f});
    envA.addDipole({{1.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}});

    MagneticEnvironment envB;
    envB.setEarthField({0.0f, 0.0f, 0.0f});
    envB.addDipole({{0.0f, 1.0f, 0.0f}, {0.0f, 10.0f, 0.0f}});

    const sf::Vec3 position{0.25f, 0.25f, 0.10f};
    const sf::Vec3 combinedField = combined.getFieldAt(position);
    const sf::Vec3 summedField = envA.getFieldAt(position) + envB.getFieldAt(position);

    EXPECT_NEAR(combinedField.x, summedField.x, 1.0e-4f);
    EXPECT_NEAR(combinedField.y, summedField.y, 1.0e-4f);
    EXPECT_NEAR(combinedField.z, summedField.z, 1.0e-4f);
}

TEST(MagneticEnvironment, PresetsProduceFiniteFieldsAndQuality) {
    const sf::Vec3 probe{0.0f, 0.0f, 0.0f};

    const MagneticEnvironment clean = MagneticEnvironment::cleanLab();
    const MagneticEnvironment office = MagneticEnvironment::officeDesk();
    const MagneticEnvironment wearable = MagneticEnvironment::wearableMotion();
    const MagneticEnvironment worst = MagneticEnvironment::worstCase();

    const MagneticEnvironment::FieldQuality cleanQuality = clean.getQualityAt(probe);
    const MagneticEnvironment::FieldQuality officeQuality = office.getQualityAt(probe);
    const MagneticEnvironment::FieldQuality wearableQuality = wearable.getQualityAt(probe);
    const MagneticEnvironment::FieldQuality worstQuality = worst.getQualityAt(probe);

    EXPECT_TRUE(std::isfinite(cleanQuality.totalFieldUt));
    EXPECT_TRUE(std::isfinite(officeQuality.totalFieldUt));
    EXPECT_TRUE(std::isfinite(wearableQuality.totalFieldUt));
    EXPECT_TRUE(std::isfinite(worstQuality.totalFieldUt));

    EXPECT_LT(cleanQuality.disturbanceRatio, 0.01f);
    EXPECT_GT(officeQuality.disturbanceRatio, cleanQuality.disturbanceRatio);
    EXPECT_GT(wearableQuality.disturbanceRatio, cleanQuality.disturbanceRatio);
    EXPECT_GT(worstQuality.disturbanceRatio, officeQuality.disturbanceRatio);
    EXPECT_TRUE(worstQuality.isDisturbed);
}

} // namespace sim
