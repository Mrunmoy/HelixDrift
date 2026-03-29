#include "VirtualMocapNodeHarness.hpp"

#include <gtest/gtest.h>

using namespace sim;

namespace {

NodeRunResult runStaticYawCase(float yawDeg) {
    VirtualMocapNodeHarness harness;
    harness.setSeed(42);
    if (!harness.initAll()) {
        ADD_FAILURE() << "VirtualMocapNodeHarness initAll() failed";
        return {};
    }

    harness.resetAndSync();
    harness.assembly().gimbal().setOrientation(sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, yawDeg));
    harness.assembly().gimbal().syncToSensors();
    return harness.runWithWarmup(100, 200, 20000);
}

NodeRunResult runDynamicYawCase(float yawRateDegPerSec) {
    VirtualMocapNodeHarness::Config config{};
    config.pipeline.mahonyKp = 0.5f;

    VirtualMocapNodeHarness harness(config);
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
    harness.assembly().gimbal().setRotationRate({0.0f, 0.0f, yawRateDegPerSec * kDegToRad});
    return harness.runWithWarmup(0, 500, 20000);
}

NodeRunResult runDynamicAxisCase(float rateXDegPerSec,
                                 float rateYDegPerSec,
                                 float rateZDegPerSec,
                                 float mahonyKp = 0.5f) {
    VirtualMocapNodeHarness::Config config{};
    config.pipeline.mahonyKp = mahonyKp;

    VirtualMocapNodeHarness harness(config);
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

TEST(PoseOrientationAccuracyTest, StaticYawWithinIntermediateBound) {
    const NodeRunResult identity = runStaticYawCase(0.0f);
    const NodeRunResult yawPos15 = runStaticYawCase(15.0f);
    const NodeRunResult yawNeg15 = runStaticYawCase(-15.0f);

    ASSERT_EQ(identity.samples.size(), 200u);
    ASSERT_EQ(yawPos15.samples.size(), 200u);
    ASSERT_EQ(yawNeg15.samples.size(), 200u);

    EXPECT_LT(identity.rmsErrorDeg, 1.0f);
    EXPECT_LT(identity.maxErrorDeg, 1.0f);

    EXPECT_LT(yawPos15.rmsErrorDeg, 20.0f);
    EXPECT_LT(yawPos15.maxErrorDeg, 25.0f);
    EXPECT_LT(yawNeg15.rmsErrorDeg, 20.0f);
    EXPECT_LT(yawNeg15.maxErrorDeg, 25.0f);
}

TEST(PoseOrientationAccuracyTest, DynamicYawTrackingWithinIntermediateBoundAtKp05) {
    const NodeRunResult yaw = runDynamicYawCase(30.0f);

    ASSERT_EQ(yaw.samples.size(), 500u);
    EXPECT_LT(yaw.rmsErrorDeg, 15.0f);
    EXPECT_LT(yaw.maxErrorDeg, 30.0f);
}

TEST(PoseOrientationAccuracyTest, DynamicRollTrackingWithinIntermediateBoundAtKp05) {
    const NodeRunResult roll = runDynamicAxisCase(30.0f, 0.0f, 0.0f, 0.5f);

    ASSERT_EQ(roll.samples.size(), 500u);
    EXPECT_LT(roll.rmsErrorDeg, 15.0f);
    EXPECT_LT(roll.maxErrorDeg, 30.0f);
}

TEST(PoseOrientationAccuracyTest, DynamicPitchTrackingAtKp05RemainsCharacterizationOnly) {
    const NodeRunResult pitch = runDynamicAxisCase(0.0f, 30.0f, 0.0f, 0.5f);

    ASSERT_EQ(pitch.samples.size(), 500u);
    EXPECT_GT(pitch.rmsErrorDeg, 15.0f);
    EXPECT_GT(pitch.maxErrorDeg, 30.0f);
}
