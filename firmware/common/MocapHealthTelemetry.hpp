#pragma once

#include "FrameCodec.hpp"
#include <cstdint>

namespace helix {

struct NodeHealthTelemetry {
    uint16_t batteryMv = 0;
    uint8_t batteryPercent = 0;
    uint8_t linkQuality = 0;
    uint16_t droppedFrames = 0;
    uint8_t calibrationState = 0;
    uint8_t flags = 0;
};

template <typename Notifier, typename Codec = sf::FrameCodec>
class NodeHealthTelemetryEmitterT {
public:
    explicit NodeHealthTelemetryEmitterT(Notifier notifier)
        : notifier_(notifier)
    {}

    bool send(uint8_t nodeId, uint64_t timestampUs, const NodeHealthTelemetry& telemetry) {
        const size_t len = Codec::encodeNodeHealth(
            nodeId,
            timestampUs,
            telemetry.batteryMv,
            telemetry.batteryPercent,
            telemetry.linkQuality,
            telemetry.droppedFrames,
            telemetry.calibrationState,
            telemetry.flags,
            frameBuf_,
            sizeof(frameBuf_));
        if (len == 0) {
            return false;
        }
        return notifier_(frameBuf_, len);
    }

private:
    Notifier notifier_;
    uint8_t frameBuf_[Codec::MAX_FRAME_SIZE]{};
};

} // namespace helix
