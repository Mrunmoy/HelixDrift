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
