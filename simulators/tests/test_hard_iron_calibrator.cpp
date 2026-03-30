#include "HardIronCalibrator.hpp"

#include <gtest/gtest.h>

#include <array>

namespace sim {
namespace {

std::array<sf::Vec3, 6> cardinalSphereSamples(float radius, const sf::Vec3& offset) {
    return {{
        { radius + offset.x, offset.y, offset.z },
        {-radius + offset.x, offset.y, offset.z },
        { offset.x,  radius + offset.y, offset.z },
        { offset.x, -radius + offset.y, offset.z },
        { offset.x, offset.y,  radius + offset.z },
        { offset.x, offset.y, -radius + offset.z },
    }};
}

} // namespace

TEST(HardIronCalibratorTest, EstimatesOffsetFromCoveredSphereSamples) {
    const sf::Vec3 trueOffset{10.0f, -5.0f, 3.0f};
    HardIronCalibrator calibrator;
    calibrator.startCalibration();

    for (const sf::Vec3& sample : cardinalSphereSamples(47.0f, trueOffset)) {
        calibrator.addSample(sample);
    }

    const sf::Vec3 estimated = calibrator.getOffset();
    EXPECT_NEAR(estimated.x, trueOffset.x, 1.0f);
    EXPECT_NEAR(estimated.y, trueOffset.y, 1.0f);
    EXPECT_NEAR(estimated.z, trueOffset.z, 1.0f);
    EXPECT_NEAR(calibrator.getRadiusUt(), 47.0f, 1.0f);
    EXPECT_TRUE(calibrator.hasSolution());
}

TEST(HardIronCalibratorTest, CoverageDetectionStaysLowForPoorSampleDistribution) {
    HardIronCalibrator calibrator;
    calibrator.startCalibration();

    for (int i = 0; i < 12; ++i) {
        calibrator.addSample({45.0f + static_cast<float>(i), 0.0f, 0.0f});
    }

    EXPECT_LT(calibrator.getConfidence(), 0.2f);
    EXPECT_FALSE(calibrator.hasSolution());
}

TEST(HardIronCalibratorTest, ResetClearsSamplesAndConfidence) {
    HardIronCalibrator calibrator;
    calibrator.startCalibration();
    for (const sf::Vec3& sample : cardinalSphereSamples(30.0f, {4.0f, 2.0f, -6.0f})) {
        calibrator.addSample(sample);
    }

    EXPECT_GT(calibrator.getConfidence(), 0.4f);
    calibrator.startCalibration();
    EXPECT_EQ(calibrator.sampleCount(), 0u);
    EXPECT_EQ(calibrator.getConfidence(), 0.0f);
    EXPECT_EQ(calibrator.getRadiusUt(), 0.0f);
}

} // namespace sim
