#include "ClockModel.hpp"
#include "VirtualRFMedium.hpp"
#include "VirtualSyncMaster.hpp"
#include "VirtualSyncNode.hpp"

#include <gtest/gtest.h>

using namespace sim;

TEST(RFSyncRobustnessTest, SyncDegradesGracefullyWithFiftyPercentLossAndKeepsSendingFrames) {
    VirtualRFMedium medium({.baseLatencyUs = 500, .packetLossRate = 0.5f});
    VirtualSyncMaster master(medium);
    VirtualSyncNode node(1, medium, ClockModel::randomCrystal(20.0f));
    ASSERT_TRUE(node.init());

    for (int i = 0; i < 100; ++i) {
        node.advanceTimeUs(100000);
        master.advanceTimeUs(100000);
        ASSERT_TRUE(node.tick());
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }

    EXPECT_GT(node.getStats().anchorsReceived, 30u);
    EXPECT_EQ(node.getStats().framesSent, 100u);
    EXPECT_GT(master.getSyncQuality(1).framesReceived, 30u);
    EXPECT_LT(std::llabs(node.getSyncOffsetErrorUs()), 5000);
}

TEST(RFSyncRobustnessTest, RecoversAfterBurstLoss) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    VirtualSyncMaster master(medium);
    VirtualSyncNode node(1, medium, ClockModel::randomCrystal(20.0f));
    ASSERT_TRUE(node.init());

    for (int i = 0; i < 20; ++i) {
        node.advanceTimeUs(100000);
        master.advanceTimeUs(100000);
        ASSERT_TRUE(node.tick());
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }

    const int64_t errorBeforeBlackout = std::llabs(node.getSyncOffsetErrorUs());
    EXPECT_LT(errorBeforeBlackout, 1000);

    medium.triggerBurstLoss(2000000);
    for (int i = 0; i < 20; ++i) {
        node.advanceTimeUs(100000);
        master.advanceTimeUs(100000);
        ASSERT_TRUE(node.tick());
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }

    EXPECT_LT(std::llabs(node.getSyncOffsetErrorUs()), 5000);

    for (int i = 0; i < 50; ++i) {
        node.advanceTimeUs(100000);
        master.advanceTimeUs(100000);
        ASSERT_TRUE(node.tick());
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }

    EXPECT_GT(node.getStats().anchorsReceived, 20u);
    EXPECT_GT(master.getSyncQuality(1).framesReceived, 20u);
    EXPECT_LT(std::llabs(node.getSyncOffsetErrorUs()), 1000);
}
