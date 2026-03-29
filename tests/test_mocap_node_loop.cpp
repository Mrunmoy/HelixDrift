#include "MocapNodeLoop.hpp"
#include "MocapProfiles.hpp"

#include <gtest/gtest.h>

namespace {

struct FakeClock {
    uint64_t now = 0;
    uint64_t nowUs() const { return now; }
};

struct FakeQuat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct FakeSample {
    FakeQuat orientation{};
};

struct FakePipeline {
    bool stepResult = true;
    uint32_t callCount = 0;
    FakeQuat nextQuat{};

    bool step(FakeSample& out) {
        ++callCount;
        out.orientation = nextQuat;
        return stepResult;
    }
};

struct FakeTransport {
    bool sendResult = true;
    uint32_t callCount = 0;
    uint8_t lastNodeId = 0;
    uint64_t lastTimestampUs = 0;
    FakeQuat lastQuat{};

    bool sendQuaternion(uint8_t nodeId, uint64_t timestampUs, const FakeQuat& q) {
        ++callCount;
        lastNodeId = nodeId;
        lastTimestampUs = timestampUs;
        lastQuat = q;
        return sendResult;
    }
};

struct FakeHook {
    uint32_t callCount = 0;
    uint64_t lastTimestampUs = 0;
    FakeQuat replacement{0.0f, 1.0f, 0.0f, 0.0f};

    void onSample(uint64_t timestampUs, FakeSample& sample) {
        ++callCount;
        lastTimestampUs = timestampUs;
        sample.orientation = replacement;
    }
};

TEST(MocapNodeLoopTest, FirstTickIsDueAndSendsFrame) {
    FakeClock clock{};
    FakePipeline pipeline{};
    FakeTransport tx{};
    helix::MocapNodeLoopConfig cfg{};
    cfg.nodeId = 7;
    cfg.outputPeriodUs = 20000;

    helix::MocapNodeLoopT<FakeClock, FakePipeline, FakeTransport, FakeSample> loop(
        clock, pipeline, tx, cfg);

    EXPECT_TRUE(loop.tick());
    EXPECT_EQ(pipeline.callCount, 1u);
    EXPECT_EQ(tx.callCount, 1u);
    EXPECT_EQ(tx.lastNodeId, 7u);
    EXPECT_EQ(tx.lastTimestampUs, 0u);
}

TEST(MocapNodeLoopTest, DoesNotRunBeforePeriodElapses) {
    FakeClock clock{};
    FakePipeline pipeline{};
    FakeTransport tx{};
    helix::MocapNodeLoopConfig cfg{};
    cfg.outputPeriodUs = 20000;

    helix::MocapNodeLoopT<FakeClock, FakePipeline, FakeTransport, FakeSample> loop(
        clock, pipeline, tx, cfg);

    EXPECT_TRUE(loop.tick());
    clock.now = 19999;
    EXPECT_FALSE(loop.tick());
    EXPECT_EQ(pipeline.callCount, 1u);
    EXPECT_EQ(tx.callCount, 1u);
}

TEST(MocapNodeLoopTest, RunsAtConfiguredCadenceForPerformanceProfile) {
    FakeClock clock{};
    FakePipeline pipeline{};
    FakeTransport tx{};
    constexpr helix::MocapProfile profile =
        helix::selectMocapProfile(helix::MocapPowerMode::PERFORMANCE);
    helix::MocapNodeLoopConfig cfg{};
    cfg.outputPeriodUs = profile.outputPeriodUs;

    helix::MocapNodeLoopT<FakeClock, FakePipeline, FakeTransport, FakeSample> loop(
        clock, pipeline, tx, cfg);

    EXPECT_TRUE(loop.tick());
    clock.now = 10000;
    EXPECT_FALSE(loop.tick());
    clock.now = 20000;
    EXPECT_TRUE(loop.tick());
    clock.now = 39999;
    EXPECT_FALSE(loop.tick());
    clock.now = 40000;
    EXPECT_TRUE(loop.tick());
    EXPECT_EQ(pipeline.callCount, 3u);
    EXPECT_EQ(tx.callCount, 3u);
}

TEST(MocapNodeLoopTest, UsesBatteryProfilePeriod) {
    FakeClock clock{};
    FakePipeline pipeline{};
    FakeTransport tx{};
    constexpr helix::MocapProfile profile =
        helix::selectMocapProfile(helix::MocapPowerMode::BATTERY);
    helix::MocapNodeLoopConfig cfg{};
    cfg.outputPeriodUs = profile.outputPeriodUs;

    helix::MocapNodeLoopT<FakeClock, FakePipeline, FakeTransport, FakeSample> loop(
        clock, pipeline, tx, cfg);

    EXPECT_TRUE(loop.tick());
    clock.now = 24999;
    EXPECT_FALSE(loop.tick());
    clock.now = 25000;
    EXPECT_TRUE(loop.tick());
    EXPECT_EQ(pipeline.callCount, 2u);
    EXPECT_EQ(tx.callCount, 2u);
}

TEST(MocapNodeLoopTest, PipelineFailureSkipsTransportForThatTick) {
    FakeClock clock{};
    FakePipeline pipeline{};
    FakeTransport tx{};
    helix::MocapNodeLoopConfig cfg{};
    cfg.outputPeriodUs = 20000;
    helix::MocapNodeLoopT<FakeClock, FakePipeline, FakeTransport, FakeSample> loop(
        clock, pipeline, tx, cfg);

    pipeline.stepResult = false;
    EXPECT_FALSE(loop.tick());
    EXPECT_EQ(pipeline.callCount, 1u);
    EXPECT_EQ(tx.callCount, 0u);

    pipeline.stepResult = true;
    clock.now = 20000;
    EXPECT_TRUE(loop.tick());
    EXPECT_EQ(pipeline.callCount, 2u);
    EXPECT_EQ(tx.callCount, 1u);
}

TEST(MocapNodeLoopTest, SampleHookCanTransformOrientationBeforeSend) {
    FakeClock clock{};
    FakePipeline pipeline{};
    FakeTransport tx{};
    FakeHook hook{};
    helix::MocapNodeLoopConfig cfg{};
    cfg.nodeId = 3;

    helix::MocapNodeLoopT<FakeClock, FakePipeline, FakeTransport, FakeSample, FakeHook> loop(
        clock, pipeline, tx, cfg, hook);

    EXPECT_TRUE(loop.tick());
    EXPECT_EQ(tx.callCount, 1u);
    EXPECT_FLOAT_EQ(tx.lastQuat.w, 0.0f);
    EXPECT_FLOAT_EQ(tx.lastQuat.x, 1.0f);
    EXPECT_FLOAT_EQ(tx.lastQuat.y, 0.0f);
    EXPECT_FLOAT_EQ(tx.lastQuat.z, 0.0f);
}

TEST(MocapNodeLoopTest, UpdateCadenceRebasesNextTickFromNow) {
    FakeClock clock{};
    FakePipeline pipeline{};
    FakeTransport tx{};
    helix::MocapNodeLoopConfig cfg{};
    cfg.outputPeriodUs = 20000;

    helix::MocapNodeLoopT<FakeClock, FakePipeline, FakeTransport, FakeSample> loop(
        clock, pipeline, tx, cfg);

    EXPECT_TRUE(loop.tick());
    EXPECT_EQ(loop.outputPeriodUs(), 20000u);

    clock.now = 5000;
    loop.updateCadence(clock.nowUs(), 25000);
    EXPECT_EQ(loop.outputPeriodUs(), 25000u);

    clock.now = 29999;
    EXPECT_FALSE(loop.tick());
    clock.now = 30000;
    EXPECT_TRUE(loop.tick());
    EXPECT_EQ(tx.callCount, 2u);
    EXPECT_EQ(tx.lastTimestampUs, 30000u);
}

} // namespace
