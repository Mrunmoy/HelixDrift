#include "ClockModel.hpp"
#include "VirtualRFMedium.hpp"
#include "VirtualSyncMaster.hpp"
#include "VirtualSyncNode.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

using namespace sim;

namespace {

float recoverRelativeAngleDeg(const sf::Quaternion& parent, const sf::Quaternion& child) {
    const sf::Quaternion relative = parent.conjugate().multiply(child);
    float clampedW = relative.w;
    if (clampedW > 1.0f) {
        clampedW = 1.0f;
    } else if (clampedW < -1.0f) {
        clampedW = -1.0f;
    }

    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    return std::abs(2.0f * std::acos(clampedW) * kRadToDeg);
}

const ReceivedFrame* latestFrameForNode(const std::vector<ReceivedFrame>& frames, uint8_t nodeId) {
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
        if (it->nodeId == nodeId) {
            return &(*it);
        }
    }
    return nullptr;
}

const ReceivedFrame* frameForNodeSequence(const std::vector<ReceivedFrame>& frames,
                                          uint8_t nodeId,
                                          uint32_t sequence) {
    for (const auto& frame : frames) {
        if (frame.nodeId == nodeId && frame.sequenceNum == sequence) {
            return &frame;
        }
    }
    return nullptr;
}

} // namespace

TEST(BodyChainSyncTest, ThreeNodeStaticChainKeepsJointAnglesAndBoundedSkew) {
    VirtualRFMedium medium({.baseLatencyUs = 400, .jitterMaxUs = 200});
    VirtualSyncMaster master(medium, 100000);
    VirtualSyncNode shoulder(1, medium, ClockModel::randomCrystal(20.0f));
    VirtualSyncNode elbow(2, medium, ClockModel::randomCrystal(20.0f));
    VirtualSyncNode wrist(3, medium, ClockModel::randomCrystal(20.0f));

    shoulder.setSeed(41);
    elbow.setSeed(42);
    wrist.setSeed(43);

    ASSERT_TRUE(shoulder.init());
    ASSERT_TRUE(elbow.init());
    ASSERT_TRUE(wrist.init());

    shoulder.resetAndSync();
    elbow.resetAndSync();
    wrist.resetAndSync();

    elbow.harness().assembly().gimbal().setOrientation(
        sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 45.0f));
    wrist.harness().assembly().gimbal().setOrientation(
        sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 90.0f));
    shoulder.harness().assembly().gimbal().syncToSensors();
    elbow.harness().assembly().gimbal().syncToSensors();
    wrist.harness().assembly().gimbal().syncToSensors();

    for (int step = 0; step < 220; ++step) {
        shoulder.advanceTimeUs(20000);
        elbow.advanceTimeUs(20000);
        wrist.advanceTimeUs(20000);
        master.advanceTimeUs(20000);

        ASSERT_TRUE(shoulder.tick());
        ASSERT_TRUE(elbow.tick());
        ASSERT_TRUE(wrist.tick());

        if ((step % 5) == 0) {
            master.broadcastAnchor();
        }

        medium.advanceTimeUs(20000);
    }

    const auto& frames = master.getReceivedFrames();
    ASSERT_FALSE(frames.empty());

    const ReceivedFrame* shoulderFrame = latestFrameForNode(frames, 1);
    const ReceivedFrame* elbowFrame = latestFrameForNode(frames, 2);
    const ReceivedFrame* wristFrame = latestFrameForNode(frames, 3);
    ASSERT_NE(shoulderFrame, nullptr);
    ASSERT_NE(elbowFrame, nullptr);
    ASSERT_NE(wristFrame, nullptr);

    const float shoulderElbowDeg = recoverRelativeAngleDeg(
        shoulderFrame->orientation, elbowFrame->orientation);
    const float elbowWristDeg = recoverRelativeAngleDeg(
        elbowFrame->orientation, wristFrame->orientation);

    const auto minmaxRemote = std::minmax({
        shoulderFrame->estimatedRemoteTimestampUs(),
        elbowFrame->estimatedRemoteTimestampUs(),
        wristFrame->estimatedRemoteTimestampUs(),
    });
    const uint64_t skewUs = minmaxRemote.second - minmaxRemote.first;

    EXPECT_NEAR(shoulderElbowDeg, 45.0f, 8.0f);
    EXPECT_NEAR(elbowWristDeg, 45.0f, 8.0f);
    EXPECT_LT(skewUs, 3000u);
    EXPECT_GT(shoulder.getStats().anchorsReceived, 20u);
    EXPECT_GT(elbow.getStats().anchorsReceived, 20u);
    EXPECT_GT(wrist.getStats().anchorsReceived, 20u);
}

TEST(BodyChainSyncTest, ThreeNodeDynamicHingeTracksRelativeAnglesOverTime) {
    VirtualRFMedium medium({.baseLatencyUs = 400, .jitterMaxUs = 200});
    VirtualSyncMaster master(medium, 100000);
    VirtualSyncNode shoulder(1, medium, ClockModel::randomCrystal(20.0f));
    VirtualSyncNode elbow(2, medium, ClockModel::randomCrystal(20.0f));
    VirtualSyncNode wrist(3, medium, ClockModel::randomCrystal(20.0f));

    shoulder.setSeed(51);
    elbow.setSeed(52);
    wrist.setSeed(53);

    ASSERT_TRUE(shoulder.init());
    ASSERT_TRUE(elbow.init());
    ASSERT_TRUE(wrist.init());

    shoulder.resetAndSync();
    elbow.resetAndSync();
    wrist.resetAndSync();

    constexpr int kSteps = 240;
    constexpr int kWarmupSteps = 60;
    constexpr float kStepAngleDeg = 0.5f;
    constexpr float kWristLeadDeg = 30.0f;

    std::vector<float> elbowErrorsDeg;
    std::vector<float> wristErrorsDeg;
    elbowErrorsDeg.reserve(kSteps);
    wristErrorsDeg.reserve(kSteps);
    uint64_t worstSkewUs = 0;

    for (int step = 0; step < kSteps; ++step) {
        const float elbowTargetDeg = std::min(90.0f, step * kStepAngleDeg);
        const float wristTargetDeg = elbowTargetDeg + kWristLeadDeg;

        shoulder.harness().assembly().gimbal().setOrientation(sf::Quaternion{});
        elbow.harness().assembly().gimbal().setOrientation(
            sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, elbowTargetDeg));
        wrist.harness().assembly().gimbal().setOrientation(
            sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, wristTargetDeg));
        shoulder.harness().assembly().gimbal().syncToSensors();
        elbow.harness().assembly().gimbal().syncToSensors();
        wrist.harness().assembly().gimbal().syncToSensors();

        shoulder.advanceTimeUs(20000);
        elbow.advanceTimeUs(20000);
        wrist.advanceTimeUs(20000);
        master.advanceTimeUs(20000);

        ASSERT_TRUE(shoulder.tick());
        ASSERT_TRUE(elbow.tick());
        ASSERT_TRUE(wrist.tick());

        if ((step % 5) == 0) {
            master.broadcastAnchor();
        }

        medium.advanceTimeUs(20000);

        const uint32_t sequence = static_cast<uint32_t>(step);
        const auto& frames = master.getReceivedFrames();
        const ReceivedFrame* shoulderFrame = frameForNodeSequence(frames, 1, sequence);
        const ReceivedFrame* elbowFrame = frameForNodeSequence(frames, 2, sequence);
        const ReceivedFrame* wristFrame = frameForNodeSequence(frames, 3, sequence);
        ASSERT_NE(shoulderFrame, nullptr);
        ASSERT_NE(elbowFrame, nullptr);
        ASSERT_NE(wristFrame, nullptr);

        const float recoveredElbowDeg = recoverRelativeAngleDeg(
            shoulderFrame->orientation, elbowFrame->orientation);
        const float recoveredWristDeg = recoverRelativeAngleDeg(
            elbowFrame->orientation, wristFrame->orientation);

        if (step >= kWarmupSteps) {
            elbowErrorsDeg.push_back(std::abs(recoveredElbowDeg - elbowTargetDeg));
            wristErrorsDeg.push_back(std::abs(recoveredWristDeg - kWristLeadDeg));

            const auto minmaxRemote = std::minmax({
                shoulderFrame->estimatedRemoteTimestampUs(),
                elbowFrame->estimatedRemoteTimestampUs(),
                wristFrame->estimatedRemoteTimestampUs(),
            });
            worstSkewUs = std::max(worstSkewUs, minmaxRemote.second - minmaxRemote.first);
        }
    }

    const float meanElbowErrorDeg =
        std::accumulate(elbowErrorsDeg.begin(), elbowErrorsDeg.end(), 0.0f) /
        static_cast<float>(elbowErrorsDeg.size());
    const float meanWristErrorDeg =
        std::accumulate(wristErrorsDeg.begin(), wristErrorsDeg.end(), 0.0f) /
        static_cast<float>(wristErrorsDeg.size());

    EXPECT_LT(meanElbowErrorDeg, 10.0f);
    EXPECT_LT(meanWristErrorDeg, 10.0f);
    EXPECT_LT(worstSkewUs, 3000u);
}
