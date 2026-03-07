#include "MocapBleSender.hpp"

#include <gtest/gtest.h>

namespace {

class FakeBleSender final : public helix::BleSender {
public:
    bool result = true;
    uint32_t callCount = 0;
    size_t lastLen = 0;
    const uint8_t* lastData = nullptr;

    bool send(const uint8_t* data, size_t len) override {
        ++callCount;
        lastData = data;
        lastLen = len;
        return result;
    }
};

} // namespace

TEST(MocapBleSenderTest, AdapterForwardsToSender) {
    FakeBleSender fake;
    helix::BleSenderAdapter adapter(&fake);
    const uint8_t payload[3] = {1, 2, 3};

    ASSERT_TRUE(adapter.valid());
    EXPECT_TRUE(adapter(payload, sizeof(payload)));
    EXPECT_EQ(fake.callCount, 1u);
    EXPECT_EQ(fake.lastData, payload);
    EXPECT_EQ(fake.lastLen, sizeof(payload));
}

TEST(MocapBleSenderTest, AdapterReturnsFalseWhenSenderIsNull) {
    helix::BleSenderAdapter adapter(nullptr);
    const uint8_t payload[2] = {9, 9};

    EXPECT_FALSE(adapter.valid());
    EXPECT_FALSE(adapter(payload, sizeof(payload)));
}

TEST(MocapBleSenderTest, WeakSymbolSenderFallsBackToFalse) {
    helix::WeakSymbolBleSender sender;
    const uint8_t payload[1] = {7};

    EXPECT_FALSE(sender.send(payload, sizeof(payload)));
}
