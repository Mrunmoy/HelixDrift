#include "SimMetrics.hpp"

#include <gtest/gtest.h>

using namespace sim;
using namespace sf;

TEST(SimMetricsTest, AngularErrorIsZeroForIdentity) {
    Quaternion q{};
    EXPECT_FLOAT_EQ(angularErrorDeg(q, q), 0.0f);
}

TEST(SimMetricsTest, AngularErrorHandlesQuaternionDoubleCover) {
    Quaternion q = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 45.0f);
    Quaternion negQ{-q.w, -q.x, -q.y, -q.z};
    EXPECT_FLOAT_EQ(angularErrorDeg(q, negQ), 0.0f);
}

TEST(SimMetricsTest, AngularErrorMatchesKnownRightAngle) {
    Quaternion identity{};
    Quaternion yaw90 = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 90.0f);
    EXPECT_NEAR(angularErrorDeg(identity, yaw90), 90.0f, 0.001f);
}

TEST(SimMetricsTest, AngularErrorMatchesKnownHalfTurn) {
    Quaternion identity{};
    Quaternion yaw180 = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 180.0f);
    EXPECT_NEAR(angularErrorDeg(identity, yaw180), 180.0f, 0.001f);
}

TEST(SimMetricsTest, SummarizeErrorSeriesReturnsMeanRmsAndMax) {
    const std::vector<float> errorsDeg{3.0f, 4.0f};

    const ErrorSeriesStats stats = summarizeErrorSeriesDeg(errorsDeg);

    EXPECT_FLOAT_EQ(stats.meanDeg, 3.5f);
    EXPECT_NEAR(stats.rmsDeg, 3.5355339f, 1e-5f);
    EXPECT_FLOAT_EQ(stats.maxDeg, 4.0f);
}

TEST(SimMetricsTest, SummarizeErrorSeriesReturnsZerosForEmptyInput) {
    const ErrorSeriesStats stats = summarizeErrorSeriesDeg({});

    EXPECT_FLOAT_EQ(stats.meanDeg, 0.0f);
    EXPECT_FLOAT_EQ(stats.rmsDeg, 0.0f);
    EXPECT_FLOAT_EQ(stats.maxDeg, 0.0f);
}

TEST(SimMetricsTest, FirstIndexAtOrBelowFindsThresholdCrossing) {
    const std::vector<float> errorsDeg{12.0f, 8.0f, 4.0f, 2.0f};

    EXPECT_EQ(firstIndexAtOrBelowDeg(errorsDeg, 5.0f), 2);
    EXPECT_EQ(firstIndexAtOrBelowDeg(errorsDeg, 1.0f), -1);
}

TEST(SimMetricsTest, LinearDriftRateMatchesSimplePositiveSlope) {
    const std::vector<float> errorsDeg{1.0f, 2.0f, 3.0f, 4.0f};

    EXPECT_NEAR(linearDriftRateDegPerMin(errorsDeg, 60000000), 1.0f, 1e-5f);
}

TEST(SimMetricsTest, LinearDriftRateCanUseTrailingWindow) {
    const std::vector<float> errorsDeg{10.0f, 10.0f, 3.0f, 5.0f, 7.0f};

    EXPECT_NEAR(linearDriftRateDegPerMin(errorsDeg, 60000000, 0.6f), 2.0f, 1e-5f);
}

TEST(SimMetricsTest, LinearDriftRateReturnsZeroForInsufficientSamples) {
    EXPECT_FLOAT_EQ(linearDriftRateDegPerMin({}, 20000), 0.0f);
    EXPECT_FLOAT_EQ(linearDriftRateDegPerMin({5.0f}, 20000), 0.0f);
    EXPECT_FLOAT_EQ(linearDriftRateDegPerMin({5.0f, 6.0f}, 0), 0.0f);
}
