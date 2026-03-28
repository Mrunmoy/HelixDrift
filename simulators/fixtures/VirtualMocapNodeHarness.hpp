#pragma once

#include "VirtualSensorAssembly.hpp"
#include "MocapNodeLoop.hpp"
#include "TimestampSynchronizedTransport.hpp"
#include "MocapNodePipeline.hpp"
#include "SimMetrics.hpp"

#include <cassert>
#include <cstdint>
#include <cmath>
#include <vector>

namespace sim {

struct VirtualClock {
    uint64_t now = 0;

    uint64_t nowUs() const { return now; }
    void advanceUs(uint64_t deltaUs) { now += deltaUs; }
};

struct CapturedQuaternionFrame {
    uint8_t nodeId = 0;
    uint64_t timestampUs = 0;
    sf::Quaternion orientation{};
};

struct CapturedNodeSample {
    uint64_t timestampUs = 0;
    sf::Quaternion truthOrientation{};
    sf::Quaternion fusedOrientation{};
    float angularErrorDeg = 0.0f;
};

struct NodeRunResult {
    std::vector<CapturedNodeSample> samples;
    float rmsErrorDeg = 0.0f;
    float maxErrorDeg = 0.0f;
    float finalErrorDeg = 0.0f;
    float driftRateDegPerMin = 0.0f;
};

struct CaptureTransport {
    bool sendResult = true;
    std::vector<CapturedQuaternionFrame> frames;

    bool sendQuaternion(uint8_t nodeId, uint64_t timestampUs, const sf::Quaternion& q) {
        frames.push_back(CapturedQuaternionFrame{nodeId, timestampUs, q});
        return sendResult;
    }
};

struct AnchorQueue {
    bool hasAnchor = false;
    uint64_t localUs = 0;
    uint64_t remoteUs = 0;

    void push(uint64_t localTimestampUs, uint64_t remoteTimestampUs) {
        hasAnchor = true;
        localUs = localTimestampUs;
        remoteUs = remoteTimestampUs;
    }

    bool poll(uint64_t& outLocalUs, uint64_t& outRemoteUs) {
        if (!hasAnchor) {
            return false;
        }
        outLocalUs = localUs;
        outRemoteUs = remoteUs;
        hasAnchor = false;
        return true;
    }
};

struct OffsetSyncFilter {
    uint64_t offsetUs = 0;
    uint32_t observeCount = 0;

    void observeAnchor(uint64_t localUs, uint64_t remoteUs) {
        offsetUs = remoteUs - localUs;
        ++observeCount;
    }

    uint64_t toRemoteTimeUs(uint64_t localUs) const {
        return localUs + offsetUs;
    }
};

class VirtualMocapNodeHarness {
public:
    struct Config {
        uint8_t nodeId = 1;
        uint32_t outputPeriodUs = 20000;
        sf::MocapNodePipeline::Config pipeline{};
    };

    using WrappedTransport =
        helix::TimestampSynchronizedTransportT<CaptureTransport, OffsetSyncFilter, AnchorQueue>;
    using Loop =
        helix::MocapNodeLoopT<VirtualClock, sf::MocapNodePipeline, WrappedTransport, sf::MocapNodeSample>;

    VirtualMocapNodeHarness()
        : VirtualMocapNodeHarness(Config{})
    {}

    explicit VirtualMocapNodeHarness(const Config& config)
        : config_(config)
        , pipeline_(assembly_.imuDriver(), &assembly_.magDriver(), &assembly_.baroDriver(), config_.pipeline)
        , wrappedTransport_(captureTransport_, syncFilter_, anchorQueue_)
        , loop_(clock_, pipeline_, wrappedTransport_,
                helix::MocapNodeLoopConfig{config_.nodeId, config_.outputPeriodUs})
    {}

    explicit VirtualMocapNodeHarness(uint8_t nodeId, uint32_t outputPeriodUs = 20000)
        : VirtualMocapNodeHarness(Config{nodeId, outputPeriodUs, {}})
    {}

    bool initAll() { return assembly_.initAll(); }

    bool tick() { return loop_.tick(); }

    void setSeed(uint32_t seed) { assembly_.setSeed(seed); }
    void resetAndSync() { assembly_.resetAndSync(); }
    bool stepMotionAndTick(uint64_t deltaUs) {
        const float dtSeconds = static_cast<float>(deltaUs) / 1000000.0f;
        assembly_.gimbal().update(dtSeconds);
        assembly_.gimbal().syncToSensors();
        advanceTimeUs(deltaUs);
        return tick();
    }

    void advanceTimeUs(uint64_t deltaUs) {
        clock_.advanceUs(deltaUs);
        assembly_.delay().timestampUs = clock_.now;
    }

    void pushAnchor(uint64_t localTimestampUs, uint64_t remoteTimestampUs) {
        anchorQueue_.push(localTimestampUs, remoteTimestampUs);
    }

    VirtualClock& clock() { return clock_; }
    CaptureTransport& captureTransport() { return captureTransport_; }
    OffsetSyncFilter& syncFilter() { return syncFilter_; }
    VirtualSensorAssembly& assembly() { return assembly_; }
    const Config& config() const { return config_; }
    bool hasFrames() const { return !captureTransport_.frames.empty(); }
    const CapturedQuaternionFrame& lastFrame() const {
        assert(!captureTransport_.frames.empty());
        return captureTransport_.frames.back();
    }

    NodeRunResult runForDuration(uint64_t durationUs, uint64_t stepUs = 20000) {
        NodeRunResult result{};
        if (stepUs == 0) {
            return result;
        }

        const uint64_t steps = durationUs / stepUs;
        for (uint64_t i = 0; i < steps; ++i) {
            if (!stepMotionAndTick(stepUs)) {
                break;
            }

            const auto& frame = captureTransport_.frames.back();
            const sf::Quaternion truth = assembly_.gimbal().getOrientation();
            const float errorDeg = angularErrorDeg(truth, frame.orientation);
            result.samples.push_back(CapturedNodeSample{
                frame.timestampUs,
                truth,
                frame.orientation,
                errorDeg,
            });
        }

        computeSummary(result);
        return result;
    }

private:
    static void computeSummary(NodeRunResult& result) {
        if (result.samples.empty()) {
            return;
        }

        float squaredErrorSum = 0.0f;
        float maxError = 0.0f;
        for (const auto& sample : result.samples) {
            squaredErrorSum += sample.angularErrorDeg * sample.angularErrorDeg;
            if (sample.angularErrorDeg > maxError) {
                maxError = sample.angularErrorDeg;
            }
        }

        result.maxErrorDeg = maxError;
        result.rmsErrorDeg = std::sqrt(squaredErrorSum / static_cast<float>(result.samples.size()));
        result.finalErrorDeg = result.samples.back().angularErrorDeg;

        if (result.samples.size() >= 2u) {
            const uint64_t elapsedUs = result.samples.back().timestampUs - result.samples.front().timestampUs;
            if (elapsedUs > 0u) {
                const float elapsedMinutes =
                    static_cast<float>(elapsedUs) / (1000000.0f * 60.0f);
                result.driftRateDegPerMin =
                    (result.samples.back().angularErrorDeg - result.samples.front().angularErrorDeg) /
                    elapsedMinutes;
            }
        }
    }

    Config config_{};
    VirtualSensorAssembly assembly_;
    VirtualClock clock_{};
    CaptureTransport captureTransport_{};
    OffsetSyncFilter syncFilter_{};
    AnchorQueue anchorQueue_{};
    sf::MocapNodePipeline pipeline_;
    WrappedTransport wrappedTransport_;
    Loop loop_;
};

} // namespace sim
