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
