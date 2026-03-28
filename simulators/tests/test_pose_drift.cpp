#include "VirtualMocapNodeHarness.hpp"
#include "SimMetrics.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace sim;

namespace {

std::vector<float> extractErrorsDeg(const NodeRunResult& run) {
    std::vector<float> errorsDeg;
    errorsDeg.reserve(run.samples.size());
    for (const auto& sample : run.samples) {
        errorsDeg.push_back(sample.angularErrorDeg);
    }
    return errorsDeg;
}

} // namespace

TEST(PoseDriftTest, IdentityStartStaysBoundedForSixtySecondsWithIntegralFeedback) {
    VirtualMocapNodeHarness::Config config{};
    config.pipeline.mahonyKp = 1.0f;
    config.pipeline.mahonyKi = 0.02f;

    VirtualMocapNodeHarness harness(config);
    harness.setSeed(42);

    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    const NodeRunResult run = harness.runWithWarmup(50, 3000, 20000);
    const std::vector<float> errorsDeg = extractErrorsDeg(run);
    const float regressionDriftDegPerMin = linearDriftRateDegPerMin(errorsDeg, 20000, 0.5f);

    ASSERT_EQ(run.samples.size(), 3000u);
    ASSERT_FALSE(errorsDeg.empty());
    EXPECT_LT(run.maxErrorDeg, 10.0f);
    EXPECT_LT(run.finalErrorDeg, 10.0f);
    EXPECT_LT(run.driftRateDegPerMin, 5.0f);
    EXPECT_GT(run.driftRateDegPerMin, -5.0f);
    EXPECT_LT(regressionDriftDegPerMin, 5.0f);
    EXPECT_GT(regressionDriftDegPerMin, -5.0f);
}
