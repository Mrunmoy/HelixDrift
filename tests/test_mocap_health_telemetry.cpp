#include "MocapHealthTelemetry.hpp"

#include <gtest/gtest.h>
#include <cstring>

namespace {

struct CapturingNotifier {
    struct State {
        uint32_t callCount = 0;
        uint8_t frame[64]{};
        size_t frameLen = 0;
    };

    State* state = nullptr;
    bool shouldSucceed = true;

    bool operator()(const uint8_t* data, size_t len) {
        if (state != nullptr) {
            ++state->callCount;
            state->frameLen = len;
            std::memcpy(state->frame, data, len);
        }
        return shouldSucceed;
    }
};

struct FakeCodec {
    static constexpr size_t MAX_FRAME_SIZE = 64;

    static size_t encodeNodeHealth(uint8_t nodeId,
                                   uint64_t timestampUs,
                                   uint16_t batteryMv,
                                   uint8_t batteryPercent,
                                   uint8_t linkQuality,
                                   uint16_t droppedFrames,
                                   uint8_t calibrationState,
                                   uint8_t flags,
                                   uint8_t* buf,
                                   size_t bufLen) {
        if (bufLen < 10) return 0;
        buf[0] = nodeId;
        std::memcpy(&buf[1], &timestampUs, sizeof(timestampUs));
        buf[9] = static_cast<uint8_t>(
            (batteryMv > 0) + batteryPercent + linkQuality + droppedFrames + calibrationState + flags);
        return 10;
    }
};

TEST(MocapHealthTelemetryTest, EncodesAndSendsNodeHealthFrame) {
    CapturingNotifier::State state{};
    CapturingNotifier notifier{&state};
    helix::NodeHealthTelemetryEmitterT<CapturingNotifier, FakeCodec> emitter(notifier);
    helix::NodeHealthTelemetry telemetry{};
    telemetry.batteryMv = 3920;
    telemetry.batteryPercent = 81;
    telemetry.linkQuality = 95;
    telemetry.droppedFrames = 4;
    telemetry.calibrationState = 2;
    telemetry.flags = 0x5A;

    ASSERT_TRUE(emitter.send(3, 123456, telemetry));
}

TEST(MocapHealthTelemetryTest, EncodedPayloadCanBeDecoded) {
    CapturingNotifier::State state{};
    CapturingNotifier notifier{&state};
    helix::NodeHealthTelemetryEmitterT<CapturingNotifier, FakeCodec> emitter(notifier);
    helix::NodeHealthTelemetry telemetry{};
    telemetry.batteryMv = 3810;
    telemetry.batteryPercent = 70;
    telemetry.linkQuality = 88;
    telemetry.droppedFrames = 9;
    telemetry.calibrationState = 1;
    telemetry.flags = 0x0F;

    ASSERT_TRUE(emitter.send(9, 555, telemetry));
    ASSERT_EQ(state.callCount, 1u);

    EXPECT_EQ(state.frameLen, 10u);
    EXPECT_EQ(state.frame[0], 9u);
}

} // namespace
