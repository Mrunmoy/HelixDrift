#include "TimestampSynchronizedTransport.hpp"

#include <gtest/gtest.h>

namespace {

struct FakeQuat {
    float w = 1.0f;
};

struct FakeTransport {
    uint8_t lastNodeId = 0;
    uint64_t lastTimestampUs = 0;
    uint32_t callCount = 0;
    bool sendResult = true;

    bool sendQuaternion(uint8_t nodeId, uint64_t timestampUs, const FakeQuat&) {
        ++callCount;
        lastNodeId = nodeId;
        lastTimestampUs = timestampUs;
        return sendResult;
    }
};

struct FakeSync {
    uint64_t offsetUs = 0;
    uint32_t observeCount = 0;
    uint64_t lastObservedLocal = 0;
    uint64_t lastObservedRemote = 0;

    void observeAnchor(uint64_t localUs, uint64_t remoteUs) {
        ++observeCount;
        lastObservedLocal = localUs;
        lastObservedRemote = remoteUs;
        offsetUs = remoteUs - localUs;
    }

    uint64_t toRemoteTimeUs(uint64_t localUs) const { return localUs + offsetUs; }
};

struct FakeAnchorSource {
    bool hasAnchor = false;
    uint64_t localUs = 0;
    uint64_t remoteUs = 0;

    bool poll(uint64_t& outLocalUs, uint64_t& outRemoteUs) {
        if (!hasAnchor) return false;
        outLocalUs = localUs;
        outRemoteUs = remoteUs;
        hasAnchor = false;
        return true;
    }
};

TEST(TimestampSynchronizedTransportTest, SendsUsingUnchangedTimestampWithoutAnchor) {
    FakeTransport tx{};
    FakeSync sync{};
    FakeAnchorSource anchor{};
    helix::TimestampSynchronizedTransportT<FakeTransport, FakeSync, FakeAnchorSource> wrapped(
        tx, sync, anchor);

    ASSERT_TRUE(wrapped.sendQuaternion(4, 1000, FakeQuat{}));
    EXPECT_EQ(tx.callCount, 1u);
    EXPECT_EQ(tx.lastNodeId, 4u);
    EXPECT_EQ(tx.lastTimestampUs, 1000u);
    EXPECT_EQ(sync.observeCount, 0u);
}

TEST(TimestampSynchronizedTransportTest, ConsumesAnchorAndAppliesOffset) {
    FakeTransport tx{};
    FakeSync sync{};
    FakeAnchorSource anchor{};
    anchor.hasAnchor = true;
    anchor.localUs = 1000;
    anchor.remoteUs = 1900;
    helix::TimestampSynchronizedTransportT<FakeTransport, FakeSync, FakeAnchorSource> wrapped(
        tx, sync, anchor);

    ASSERT_TRUE(wrapped.sendQuaternion(2, 2000, FakeQuat{}));
    EXPECT_EQ(sync.observeCount, 1u);
    EXPECT_EQ(sync.lastObservedLocal, 1000u);
    EXPECT_EQ(sync.lastObservedRemote, 1900u);
    EXPECT_EQ(tx.lastTimestampUs, 2900u);
}

TEST(TimestampSynchronizedTransportTest, ReusesLastSyncOffsetAfterAnchorConsumed) {
    FakeTransport tx{};
    FakeSync sync{};
    FakeAnchorSource anchor{};
    anchor.hasAnchor = true;
    anchor.localUs = 1000;
    anchor.remoteUs = 1500;
    helix::TimestampSynchronizedTransportT<FakeTransport, FakeSync, FakeAnchorSource> wrapped(
        tx, sync, anchor);

    ASSERT_TRUE(wrapped.sendQuaternion(1, 1200, FakeQuat{}));
    ASSERT_TRUE(wrapped.sendQuaternion(1, 1300, FakeQuat{}));
    EXPECT_EQ(sync.observeCount, 1u);
    EXPECT_EQ(tx.callCount, 2u);
    EXPECT_EQ(tx.lastTimestampUs, 1800u);
}

} // namespace
