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
    harness.assembly().gimbal().setRotationRate({0.0f, 0.0f, yawRateDegPerSec * kDegToRad});
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

TEST(PoseOrientationAccuracyTest, DynamicYawTrackingWithinLooseBound) {
    const NodeRunResult yaw = runDynamicYawCase(30.0f);

    ASSERT_EQ(yaw.samples.size(), 500u);
    EXPECT_LT(yaw.rmsErrorDeg, 30.0f);
    EXPECT_LT(yaw.maxErrorDeg, 40.0f);
}
