#include "VirtualMocapNodeHarness.hpp"
#include "SimMetrics.hpp"

#include <gtest/gtest.h>
#include <cmath>

using namespace sim;

TEST(VirtualMocapNodeHarnessTest, EmitsQuaternionFramesAtConfiguredCadence) {
    VirtualMocapNodeHarness harness(7, 20000);
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    EXPECT_TRUE(harness.tick());
    EXPECT_EQ(harness.captureTransport().frames.size(), 1u);
    EXPECT_EQ(harness.captureTransport().frames.back().nodeId, 7u);
    EXPECT_EQ(harness.captureTransport().frames.back().timestampUs, 0u);

    harness.advanceTimeUs(19999);
    EXPECT_FALSE(harness.tick());
    EXPECT_EQ(harness.captureTransport().frames.size(), 1u);

    harness.advanceTimeUs(1);
    EXPECT_TRUE(harness.tick());
    EXPECT_EQ(harness.captureTransport().frames.size(), 2u);
    EXPECT_EQ(harness.captureTransport().frames.back().timestampUs, 20000u);
}

TEST(VirtualMocapNodeHarnessTest, CapturesFiniteQuaternionFromRealPipeline) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    ASSERT_TRUE(harness.tick());
    ASSERT_FALSE(harness.captureTransport().frames.empty());

    const auto& frame = harness.captureTransport().frames.back();
    EXPECT_TRUE(std::isfinite(frame.orientation.w));
    EXPECT_TRUE(std::isfinite(frame.orientation.x));
    EXPECT_TRUE(std::isfinite(frame.orientation.y));
    EXPECT_TRUE(std::isfinite(frame.orientation.z));
}

TEST(VirtualMocapNodeHarnessTest, AnchorMapsLocalTimestampIntoRemoteTime) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    harness.advanceTimeUs(1000);
    harness.pushAnchor(1000, 5000);

    ASSERT_TRUE(harness.tick());
    ASSERT_FALSE(harness.captureTransport().frames.empty());
    EXPECT_EQ(harness.syncFilter().observeCount, 1u);
    EXPECT_EQ(harness.captureTransport().frames.back().timestampUs, 5000u);
}

TEST(VirtualMocapNodeHarnessTest, FlatPoseStaysWithinBoundedAngularError) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(harness.tick());
        if (i != 4) {
            harness.advanceTimeUs(20000);
        }
    }

    const auto& frame = harness.captureTransport().frames.back();
    const float errorDeg =
        angularErrorDeg(harness.assembly().gimbal().getOrientation(), frame.orientation);

    EXPECT_TRUE(std::isfinite(errorDeg));
    EXPECT_LT(errorDeg, 15.0f);
}

TEST(VirtualMocapNodeHarnessTest, ConstantYawMotionStaysWithinBoundedErrorForShortRun) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();
    harness.assembly().gimbal().setRotationRate({0.0f, 0.0f, 0.314f});

    for (int i = 0; i < 25; ++i) {
        ASSERT_TRUE(harness.stepMotionAndTick(20000));
    }

    const float errorDeg =
        angularErrorDeg(harness.assembly().gimbal().getOrientation(), harness.lastFrame().orientation);

    EXPECT_TRUE(std::isfinite(errorDeg));
    EXPECT_LT(errorDeg, 35.0f);
}

TEST(VirtualMocapNodeHarnessTest, StaticQuarterTurnConvergesWithinBoundedError) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();
    harness.assembly().gimbal().setOrientation(sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 45.0f));
    harness.assembly().gimbal().syncToSensors();

    for (int i = 0; i < 20; ++i) {
        ASSERT_TRUE(harness.tick());
        if (i != 19) {
            harness.advanceTimeUs(20000);
        }
    }

    const float errorDeg =
        angularErrorDeg(harness.assembly().gimbal().getOrientation(), harness.lastFrame().orientation);

    EXPECT_TRUE(std::isfinite(errorDeg));
    EXPECT_LT(errorDeg, 55.0f);
}

TEST(VirtualMocapNodeHarnessTest, RunForDurationCollectsExpectedSamplesAndStats) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    const NodeRunResult result = harness.runForDuration(100000, 20000);

    ASSERT_EQ(result.samples.size(), 5u);
    EXPECT_EQ(result.samples.front().timestampUs, 20000u);
    EXPECT_EQ(result.samples.back().timestampUs, 100000u);
    EXPECT_TRUE(std::isfinite(result.rmsErrorDeg));
    EXPECT_TRUE(std::isfinite(result.maxErrorDeg));
    EXPECT_GE(result.maxErrorDeg, 0.0f);
    EXPECT_GE(result.rmsErrorDeg, 0.0f);
}

TEST(VirtualMocapNodeHarnessTest, RunForDurationTracksShortYawMotionWithFiniteErrors) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();
    harness.assembly().gimbal().setRotationRate({0.0f, 0.0f, 0.314f});

    const NodeRunResult result = harness.runForDuration(200000, 20000);

    ASSERT_EQ(result.samples.size(), 10u);
    EXPECT_TRUE(std::isfinite(result.rmsErrorDeg));
    EXPECT_TRUE(std::isfinite(result.maxErrorDeg));
    EXPECT_LT(result.maxErrorDeg, 40.0f);
}
