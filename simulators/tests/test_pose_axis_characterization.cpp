#include "VirtualMocapNodeHarness.hpp"

#include <gtest/gtest.h>

using namespace sim;

namespace {

NodeRunResult runStaticAxisOffsetCase(const sf::Quaternion& truth) {
    VirtualMocapNodeHarness harness;
    harness.setSeed(42);
    if (!harness.initAll()) {
        ADD_FAILURE() << "VirtualMocapNodeHarness initAll() failed";
        return {};
    }

    harness.resetAndSync();
    harness.assembly().gimbal().setOrientation(truth);
    harness.assembly().gimbal().syncToSensors();
    return harness.runWithWarmup(100, 200, 20000);
}

NodeRunResult runDynamicAxisTrackingCase(float rateXDegPerSec,
                                         float rateYDegPerSec,
                                         float rateZDegPerSec) {
    VirtualMocapNodeHarness harness;
    harness.setSeed(42);
    if (!harness.initAll()) {
        ADD_FAILURE() << "VirtualMocapNodeHarness initAll() failed";
        return {};
    }

    harness.resetAndSync();
    for (int i = 0; i < 50; ++i) {
        if (!harness.stepMotionAndTick(20000)) {
            ADD_FAILURE() << "Warmup stepMotionAndTick() failed";
            return {};
        }
    }

    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    harness.assembly().gimbal().setRotationRate(
        {rateXDegPerSec * kDegToRad, rateYDegPerSec * kDegToRad, rateZDegPerSec * kDegToRad});
    return harness.runWithWarmup(0, 500, 20000);
}

} // namespace

TEST(PoseAxisCharacterizationTest, SmallStaticOffsetsSeedAccuratelyAcrossAxes) {
    const NodeRunResult yaw =
        runStaticAxisOffsetCase(sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 15.0f));
    const NodeRunResult pitch =
        runStaticAxisOffsetCase(sf::Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, 15.0f));
    const NodeRunResult roll =
        runStaticAxisOffsetCase(sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 15.0f));

    ASSERT_EQ(yaw.samples.size(), 200u);
    ASSERT_EQ(pitch.samples.size(), 200u);
    ASSERT_EQ(roll.samples.size(), 200u);

    EXPECT_LT(yaw.rmsErrorDeg, 1.0f);
    EXPECT_LT(yaw.maxErrorDeg, 1.0f);
    EXPECT_LT(pitch.rmsErrorDeg, 1.0f);
    EXPECT_LT(pitch.maxErrorDeg, 1.0f);
    EXPECT_LT(roll.rmsErrorDeg, 1.0f);
    EXPECT_LT(roll.maxErrorDeg, 1.0f);
}

TEST(PoseAxisCharacterizationTest, DynamicYawTrackingRemainsEasierThanPitchAndRoll) {
    const NodeRunResult yaw = runDynamicAxisTrackingCase(0.0f, 0.0f, 30.0f);
    const NodeRunResult pitch = runDynamicAxisTrackingCase(0.0f, 30.0f, 0.0f);
    const NodeRunResult roll = runDynamicAxisTrackingCase(30.0f, 0.0f, 0.0f);

    ASSERT_EQ(yaw.samples.size(), 500u);
    ASSERT_EQ(pitch.samples.size(), 500u);
    ASSERT_EQ(roll.samples.size(), 500u);

    EXPECT_LT(yaw.rmsErrorDeg, pitch.rmsErrorDeg);
    EXPECT_LT(yaw.rmsErrorDeg, roll.rmsErrorDeg);
    EXPECT_LT(yaw.maxErrorDeg, pitch.maxErrorDeg);
    EXPECT_LT(yaw.maxErrorDeg, roll.maxErrorDeg);
}
