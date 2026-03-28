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

TEST(VirtualMocapNodeHarnessTest, SameSeedProducesDeterministicRunStatisticsAcrossHarnesses) {
    VirtualMocapNodeHarness harnessA;
    VirtualMocapNodeHarness harnessB;
    harnessA.setSeed(1357);
    harnessB.setSeed(1357);

    harnessA.assembly().imuSim().setAccelNoiseStdDev(0.01f);
    harnessB.assembly().imuSim().setAccelNoiseStdDev(0.01f);
    harnessA.assembly().imuSim().setGyroNoiseStdDev(0.005f);
    harnessB.assembly().imuSim().setGyroNoiseStdDev(0.005f);
    harnessA.assembly().baroSim().setPressureNoiseStdDev(0.1f);
    harnessB.assembly().baroSim().setPressureNoiseStdDev(0.1f);

    Bmm350Simulator::ErrorConfig magErrors{};
    magErrors.noiseStdDev = 0.2f;
    harnessA.assembly().magSim().setErrors(magErrors);
    harnessB.assembly().magSim().setErrors(magErrors);

    ASSERT_TRUE(harnessA.initAll());
    ASSERT_TRUE(harnessB.initAll());
    harnessA.resetAndSync();
    harnessB.resetAndSync();
    harnessA.assembly().gimbal().setRotationRate({0.05f, 0.0f, 0.2f});
    harnessB.assembly().gimbal().setRotationRate({0.05f, 0.0f, 0.2f});

    const NodeRunResult resultA = harnessA.runForDuration(200000, 20000);
    const NodeRunResult resultB = harnessB.runForDuration(200000, 20000);

    ASSERT_EQ(resultA.samples.size(), resultB.samples.size());
    ASSERT_FALSE(resultA.samples.empty());
    EXPECT_FLOAT_EQ(resultA.rmsErrorDeg, resultB.rmsErrorDeg);
    EXPECT_FLOAT_EQ(resultA.maxErrorDeg, resultB.maxErrorDeg);
    EXPECT_FLOAT_EQ(resultA.finalErrorDeg, resultB.finalErrorDeg);
    EXPECT_FLOAT_EQ(resultA.driftRateDegPerMin, resultB.driftRateDegPerMin);
}

TEST(VirtualMocapNodeHarnessTest, RunForDurationComputesFinalAndDriftMetrics) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();
    harness.assembly().gimbal().setRotationRate({0.0f, 0.0f, 0.314f});

    const NodeRunResult result = harness.runForDuration(200000, 20000);

    ASSERT_FALSE(result.samples.empty());
    EXPECT_FLOAT_EQ(result.finalErrorDeg, result.samples.back().angularErrorDeg);
    EXPECT_TRUE(std::isfinite(result.driftRateDegPerMin));
}

TEST(VirtualMocapNodeHarnessTest, EmptyHarnessHasNoFrames) {
    VirtualMocapNodeHarness harness;

    EXPECT_FALSE(harness.hasFrames());
}

TEST(VirtualMocapNodeHarnessTest, LastFrameDiesWhenNoFramesHaveBeenCaptured) {
    VirtualMocapNodeHarness harness;

    EXPECT_DEATH(static_cast<void>(harness.lastFrame()), "");
}

TEST(VirtualMocapNodeHarnessTest, ConfigConstructorUsesConfiguredNodeIdAndCadence) {
    VirtualMocapNodeHarness::Config config{};
    config.nodeId = 9;
    config.outputPeriodUs = 10000;
    config.pipeline.preferMag = false;

    VirtualMocapNodeHarness harness(config);
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    EXPECT_EQ(harness.config().nodeId, 9u);
    EXPECT_EQ(harness.config().outputPeriodUs, 10000u);
    EXPECT_FALSE(harness.config().pipeline.preferMag);

    ASSERT_TRUE(harness.tick());
    ASSERT_TRUE(harness.hasFrames());
    EXPECT_EQ(harness.lastFrame().nodeId, 9u);
    EXPECT_EQ(harness.lastFrame().timestampUs, 0u);

    harness.advanceTimeUs(9999);
    EXPECT_FALSE(harness.tick());
    harness.advanceTimeUs(1);
    EXPECT_TRUE(harness.tick());
    EXPECT_EQ(harness.lastFrame().timestampUs, 10000u);
}

TEST(VirtualMocapNodeHarnessTest, RunForDurationReturnsEmptyResultWhenStepIsZero) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());

    const NodeRunResult result = harness.runForDuration(100000, 0);

    EXPECT_TRUE(result.samples.empty());
    EXPECT_EQ(result.rmsErrorDeg, 0.0f);
    EXPECT_EQ(result.maxErrorDeg, 0.0f);
    EXPECT_EQ(result.finalErrorDeg, 0.0f);
    EXPECT_EQ(result.driftRateDegPerMin, 0.0f);
}
