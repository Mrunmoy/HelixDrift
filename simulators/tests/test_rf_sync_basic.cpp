#include "ClockModel.hpp"
#include "VirtualRFMedium.hpp"
#include "VirtualSyncMaster.hpp"
#include "VirtualSyncNode.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace sim;

TEST(RFSyncBasicTest, NodeEstimatesOffsetFromSingleAnchor) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    VirtualSyncMaster master(medium);
    VirtualSyncNode node(1, medium, {.offsetUs = 5000});
    ASSERT_TRUE(node.init());

    node.advanceTimeUs(100000);
    master.advanceTimeUs(100000);
    master.broadcastAnchor();
    medium.advanceTimeUs(1000);

    EXPECT_NEAR(static_cast<double>(node.getSyncOffsetUs()), 5000.0, 1.0);
    EXPECT_NEAR(static_cast<double>(node.getSyncOffsetErrorUs()), 0.0, 1.0);
}

TEST(RFSyncBasicTest, SyncDegradesGracefullyWithFiftyPercentLoss) {
    VirtualRFMedium medium({.baseLatencyUs = 500, .packetLossRate = 0.5f});
    VirtualSyncMaster master(medium);
    VirtualSyncNode node(1, medium, ClockModel::randomCrystal(20.0f));
    ASSERT_TRUE(node.init());

    for (int i = 0; i < 100; ++i) {
        node.advanceTimeUs(100000);
        master.advanceTimeUs(100000);
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }

    EXPECT_GT(node.getStats().anchorsReceived, 30u);
    EXPECT_LT(std::llabs(node.getSyncOffsetErrorUs()), 5000);
}

TEST(RFSyncBasicTest, SixNodesConvergeUnderRepeatedAnchors) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    VirtualSyncMaster master(medium);

    std::vector<std::unique_ptr<VirtualSyncNode>> nodes;
    for (int i = 1; i <= 6; ++i) {
        nodes.push_back(std::make_unique<VirtualSyncNode>(
            static_cast<uint8_t>(i), medium, ClockModel::randomCrystal()));
        ASSERT_TRUE(nodes.back()->init());
    }

    for (int i = 0; i < 100; ++i) {
        for (auto& node : nodes) {
            node->advanceTimeUs(100000);
        }
        master.advanceTimeUs(100000);
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }

    for (const auto& node : nodes) {
        EXPECT_GT(node->getStats().anchorsReceived, 0u);
        EXPECT_LT(std::llabs(node->getSyncOffsetErrorUs()), 1000);
    }
}
