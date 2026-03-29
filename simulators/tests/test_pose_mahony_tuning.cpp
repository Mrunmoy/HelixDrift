#include "VirtualMocapNodeHarness.hpp"
#include "SimMetrics.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace sim;

namespace {

struct BiasRunMetrics {
    NodeRunResult run;
    float driftRateDegPerMin = 0.0f;
};

BiasRunMetrics runStationaryBiasCase(float gyroBiasZRadPerSec, float mahonyKp, float mahonyKi) {
    VirtualMocapNodeHarness::Config config{};
    config.pipeline.mahonyKp = mahonyKp;
    config.pipeline.mahonyKi = mahonyKi;

    VirtualMocapNodeHarness harness(config);
    harness.setSeed(42);
    if (!harness.initAll()) {
        ADD_FAILURE() << "VirtualMocapNodeHarness initAll() failed";
        return {};
    }
    harness.resetAndSync();
    harness.assembly().imuSim().setGyroBias({0.0f, 0.0f, gyroBiasZRadPerSec});

    BiasRunMetrics metrics{};
    metrics.run = harness.runWithWarmup(50, 1500, 20000);

    std::vector<float> errorsDeg;
    errorsDeg.reserve(metrics.run.samples.size());
    for (const auto& sample : metrics.run.samples) {
        errorsDeg.push_back(sample.angularErrorDeg);
    }
    metrics.driftRateDegPerMin = linearDriftRateDegPerMin(errorsDeg, 20000, 0.5f);
    return metrics;
}

} // namespace

TEST(PoseMahonyTuningTest, GyroZBiasWithoutIntegralFeedbackShowsPositiveHeadingDrift) {
    const BiasRunMetrics baseline = runStationaryBiasCase(0.01f, 1.0f, 0.0f);

    ASSERT_EQ(baseline.run.samples.size(), 1500u);
    EXPECT_GT(baseline.driftRateDegPerMin, 0.5f); // Clean-field drift should still be meaningfully positive.
    EXPECT_GT(baseline.run.finalErrorDeg, 3.0f);
    EXPECT_GT(baseline.run.maxErrorDeg, 3.0f);
}

TEST(PoseMahonyTuningTest, ModerateIntegralFeedbackCanImproveGyroZBiasRecoveryAtKp05) {
    const BiasRunMetrics noIntegral = runStationaryBiasCase(0.01f, 0.5f, 0.0f);
    const BiasRunMetrics ki005 = runStationaryBiasCase(0.01f, 0.5f, 0.05f);

    ASSERT_EQ(noIntegral.run.samples.size(), 1500u);
    ASSERT_EQ(ki005.run.samples.size(), 1500u);

    EXPECT_LT(ki005.run.finalErrorDeg, noIntegral.run.finalErrorDeg);
    EXPECT_LT(ki005.run.rmsErrorDeg, noIntegral.run.rmsErrorDeg);
    EXPECT_LT(ki005.driftRateDegPerMin, noIntegral.driftRateDegPerMin);
    EXPECT_LT(ki005.run.finalErrorDeg, 20.0f);
}

TEST(PoseMahonyTuningTest, GyroXBiasRemainsHarderToRejectThanGyroZBiasInCurrentHarness) {
    VirtualMocapNodeHarness::Config config{};
    config.pipeline.mahonyKp = 0.5f;
    config.pipeline.mahonyKi = 0.05f;

    VirtualMocapNodeHarness xBiasHarness(config);
    VirtualMocapNodeHarness zBiasHarness(config);
    xBiasHarness.setSeed(42);
    zBiasHarness.setSeed(42);

    ASSERT_TRUE(xBiasHarness.initAll());
    ASSERT_TRUE(zBiasHarness.initAll());
    xBiasHarness.resetAndSync();
    zBiasHarness.resetAndSync();

    xBiasHarness.assembly().imuSim().setGyroBias({0.01f, 0.0f, 0.0f});
    zBiasHarness.assembly().imuSim().setGyroBias({0.0f, 0.0f, 0.01f});

    const NodeRunResult xBias = xBiasHarness.runWithWarmup(50, 1500, 20000);
    const NodeRunResult zBias = zBiasHarness.runWithWarmup(50, 1500, 20000);

    ASSERT_EQ(xBias.samples.size(), 1500u);
    ASSERT_EQ(zBias.samples.size(), 1500u);
    // After the SensorFusion convention fix, the current clean-field simulator
    // shows X-axis gyro bias as the harder case. Keep this as characterization,
    // not a universal physical claim.
    EXPECT_GT(xBias.finalErrorDeg, zBias.finalErrorDeg);
}
