#include "VirtualRFMedium.hpp"

#include <gtest/gtest.h>

using namespace sim;

TEST(VirtualRFMediumTest, SinglePacketDeliveredWithLatency) {
    VirtualRFMedium medium({.baseLatencyUs = 500});

    bool received = false;
    Packet receivedPacket{};
    uint64_t receivedTimeUs = 0;

    medium.registerNode(1, [&](const Packet& packet, uint64_t rxTimestampUs) {
        received = true;
        receivedPacket = packet;
        receivedTimeUs = rxTimestampUs;
    });

    medium.transmit(2, Packet{.dstId = 1, .txTimestampUs = 1000, .payload = {1, 2, 3}});
    EXPECT_FALSE(received);

    medium.advanceTimeUs(499);
    EXPECT_FALSE(received);

    medium.advanceTimeUs(1);
    ASSERT_TRUE(received);
    EXPECT_EQ(receivedPacket.srcId, 2u);
    EXPECT_EQ(receivedPacket.dstId, 1u);
    EXPECT_EQ(receivedPacket.payload.size(), 3u);
    EXPECT_EQ(receivedTimeUs, 1500u);

    const auto stats = medium.getStats();
    EXPECT_EQ(stats.packetsTransmitted, 1u);
    EXPECT_EQ(stats.packetsDelivered, 1u);
    EXPECT_EQ(stats.packetsLost, 0u);
}

TEST(VirtualRFMediumTest, BroadcastPacketDeliveredToAllNodes) {
    VirtualRFMedium medium({.baseLatencyUs = 100});

    bool node1Received = false;
    bool node2Received = false;
    bool node3Received = false;

    medium.registerNode(1, [&](const Packet&, uint64_t) { node1Received = true; });
    medium.registerNode(2, [&](const Packet&, uint64_t) { node2Received = true; });
    medium.registerNode(3, [&](const Packet&, uint64_t) { node3Received = true; });

    medium.transmit(9, Packet{
        .dstId = VirtualRFMedium::kBroadcastNodeId,
        .txTimestampUs = 0,
        .payload = {0xAB},
    });

    medium.advanceTimeUs(100);

    EXPECT_TRUE(node1Received);
    EXPECT_TRUE(node2Received);
    EXPECT_TRUE(node3Received);

    const auto stats = medium.getStats();
    EXPECT_EQ(stats.packetsTransmitted, 1u);
    EXPECT_EQ(stats.packetsDelivered, 3u);
    EXPECT_EQ(stats.packetsLost, 0u);
}

TEST(VirtualRFMediumTest, PacketLossAtConfiguredRateDropsPacketsDeterministically) {
    VirtualRFMedium medium({.baseLatencyUs = 100, .packetLossRate = 1.0f});

    uint32_t receivedCount = 0;
    medium.registerNode(1, [&](const Packet&, uint64_t) { ++receivedCount; });

    for (int i = 0; i < 10; ++i) {
        medium.transmit(2, Packet{.dstId = 1, .txTimestampUs = static_cast<uint64_t>(i * 1000)});
    }

    medium.advanceTimeUs(1000);

    EXPECT_EQ(receivedCount, 0u);
    const auto stats = medium.getStats();
    EXPECT_EQ(stats.packetsTransmitted, 10u);
    EXPECT_EQ(stats.packetsDelivered, 0u);
    EXPECT_EQ(stats.packetsLost, 10u);
}
