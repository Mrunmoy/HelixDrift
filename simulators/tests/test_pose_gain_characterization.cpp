#include "VirtualMocapNodeHarness.hpp"

#include <gtest/gtest.h>

using namespace sim;

namespace {

NodeRunResult runStaticYawOffsetCase(float yawDeg, float mahonyKp) {
    VirtualMocapNodeHarness::Config config{};
    config.pipeline.mahonyKp = mahonyKp;

    VirtualMocapNodeHarness harness(config);
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

NodeRunResult runDynamicYawTrackingCase(float yawRateDegPerSec, float mahonyKp) {
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
    harness.assembly().gimbal().setRotationRate({0.0f, 0.0f, yawRateDegPerSec * kDegToRad});
    return harness.runWithWarmup(0, 500, 20000);
}

} // namespace

TEST(PoseGainCharacterizationTest, VeryHighStaticYawGainDestabilizesAnOtherwiseAccurateSeed) {
    const NodeRunResult kp05 = runStaticYawOffsetCase(15.0f, 0.5f);
    const NodeRunResult kp10 = runStaticYawOffsetCase(15.0f, 1.0f);
    const NodeRunResult kp20 = runStaticYawOffsetCase(15.0f, 2.0f);
    const NodeRunResult kp50 = runStaticYawOffsetCase(15.0f, 5.0f);

    ASSERT_EQ(kp05.samples.size(), 200u);
    ASSERT_EQ(kp10.samples.size(), 200u);
    ASSERT_EQ(kp20.samples.size(), 200u);
    ASSERT_EQ(kp50.samples.size(), 200u);

    EXPECT_LT(kp05.rmsErrorDeg, 1.0f);
    EXPECT_LT(kp05.maxErrorDeg, 1.0f);
    EXPECT_LT(kp10.rmsErrorDeg, 1.0f);
    EXPECT_LT(kp10.maxErrorDeg, 1.0f);
    EXPECT_LT(kp20.rmsErrorDeg, 1.0f);
    EXPECT_LT(kp20.maxErrorDeg, 1.0f);

    EXPECT_GT(kp50.rmsErrorDeg, 40.0f);
    EXPECT_GT(kp50.finalErrorDeg, 20.0f);
}

TEST(PoseGainCharacterizationTest, VeryHighYawGainDegradesDynamicTracking) {
    const NodeRunResult kp05 = runDynamicYawTrackingCase(30.0f, 0.5f);
    const NodeRunResult kp10 = runDynamicYawTrackingCase(30.0f, 1.0f);
    const NodeRunResult kp20 = runDynamicYawTrackingCase(30.0f, 2.0f);
    const NodeRunResult kp50 = runDynamicYawTrackingCase(30.0f, 5.0f);

    ASSERT_EQ(kp05.samples.size(), 500u);
    ASSERT_EQ(kp10.samples.size(), 500u);
    ASSERT_EQ(kp20.samples.size(), 500u);
    ASSERT_EQ(kp50.samples.size(), 500u);

    EXPECT_LT(kp05.rmsErrorDeg, 10.0f);
    EXPECT_LT(kp05.maxErrorDeg, 5.0f);
    EXPECT_LT(kp10.rmsErrorDeg, 15.0f);
    EXPECT_LT(kp10.maxErrorDeg, 15.0f);
    EXPECT_LT(kp20.rmsErrorDeg, 10.0f);
    EXPECT_LT(kp20.maxErrorDeg, 10.0f);

    EXPECT_GT(kp50.rmsErrorDeg, 20.0f);
    EXPECT_GT(kp50.maxErrorDeg, 50.0f);
}
