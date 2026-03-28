#pragma once

#include "VirtualSensorAssembly.hpp"
#include "MocapNodeLoop.hpp"
#include "TimestampSynchronizedTransport.hpp"
#include "MocapNodePipeline.hpp"

#include <cstdint>
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
    using WrappedTransport =
        helix::TimestampSynchronizedTransportT<CaptureTransport, OffsetSyncFilter, AnchorQueue>;
    using Loop =
        helix::MocapNodeLoopT<VirtualClock, sf::MocapNodePipeline, WrappedTransport, sf::MocapNodeSample>;

    explicit VirtualMocapNodeHarness(uint8_t nodeId = 1, uint32_t outputPeriodUs = 20000)
        : pipeline_(assembly_.imuDriver(), &assembly_.magDriver(), &assembly_.baroDriver())
        , wrappedTransport_(captureTransport_, syncFilter_, anchorQueue_)
        , loop_(clock_, pipeline_, wrappedTransport_, helix::MocapNodeLoopConfig{nodeId, outputPeriodUs})
    {}

    bool initAll() { return assembly_.initAll(); }

    bool tick() { return loop_.tick(); }

    void resetAndSync() { assembly_.resetAndSync(); }
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

private:
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
