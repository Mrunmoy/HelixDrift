#include "ClockModel.hpp"
#include "RFSyncProtocol.hpp"
#include "VirtualRFMedium.hpp"
#include "VirtualSyncNode.hpp"

#include <gtest/gtest.h>

using namespace sim;

TEST(VirtualSyncNodeTest, LocalTimeDriftsFromTrueTime) {
    VirtualRFMedium medium({});
    ClockModel clock{.driftPpm = 1000.0f};

    VirtualSyncNode node(1, medium, clock);
    node.advanceTimeUs(1000000);

    EXPECT_NEAR(static_cast<double>(node.localTimeUs()), 1001000.0, 1.0);
}

TEST(VirtualSyncNodeTest, TransmitsWithLocalTimestamp) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    ClockModel clock{.offsetUs = 5000};

    Packet received{};
    uint64_t receivedAtUs = 0;
    medium.registerNode(0, [&](const Packet& packet, uint64_t rxTimestampUs) {
        received = packet;
        receivedAtUs = rxTimestampUs;
    });

    VirtualSyncNode node(1, medium, clock);
    ASSERT_TRUE(node.init());
    node.advanceTimeUs(20000);
    ASSERT_TRUE(node.tick());

    medium.advanceTimeUs(500);

    ASSERT_EQ(received.srcId, 1u);
    EXPECT_EQ(received.txTimestampUs, node.localTimeUs());

    rfsync::FramePayload payload{};
    ASSERT_TRUE(rfsync::decodeFrame(received, payload));
    EXPECT_EQ(payload.localTimestampUs, node.localTimeUs());
    EXPECT_EQ(receivedAtUs, node.localTimeUs() + 500u);
}

TEST(VirtualSyncNodeTest, ReceivesAndRecordsRxTimestamp) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    ClockModel clock{.offsetUs = 5000};

    VirtualSyncNode node(1, medium, clock);
    ASSERT_TRUE(node.init());

    medium.transmit(0, Packet{
        .dstId = 1,
        .txTimestampUs = 0,
        .payload = rfsync::encodeAnchor({.sequence = 1, .masterTimestampUs = 0}),
    });

    medium.advanceTimeUs(500);

    EXPECT_EQ(node.lastRxTimestampLocalUs(), 5500u);
    EXPECT_EQ(node.getStats().anchorsReceived, 1u);
}
