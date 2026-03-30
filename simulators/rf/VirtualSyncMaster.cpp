#include "VirtualSyncMaster.hpp"

#include <cmath>

namespace sim {

namespace {

float computeStdDev(const std::vector<int64_t>& samples) {
    if (samples.size() < 2u) {
        return 0.0f;
    }
    long double sum = 0.0;
    for (const auto sample : samples) {
        sum += static_cast<long double>(sample);
    }
    const long double mean = sum / static_cast<long double>(samples.size());

    long double variance = 0.0;
    for (const auto sample : samples) {
        const long double delta = static_cast<long double>(sample) - mean;
        variance += delta * delta;
    }
    variance /= static_cast<long double>(samples.size());
    return static_cast<float>(std::sqrt(variance));
}

} // namespace

VirtualSyncMaster::VirtualSyncMaster(VirtualRFMedium& medium, uint32_t anchorPeriodUs)
    : medium_(medium), anchorPeriodUs_(anchorPeriodUs) {
    medium_.registerNode(kMasterNodeId, [this](const Packet& packet, uint64_t rxTimestampUs) {
        onPacketReceived(packet, rxTimestampUs);
    });
}

void VirtualSyncMaster::advanceTimeUs(uint64_t deltaUs) {
    localTimeUs_ += deltaUs;
}

void VirtualSyncMaster::broadcastAnchor() {
    const uint64_t timestampUs = medium_.nowUs();
    localTimeUs_ = timestampUs;

    rfsync::AnchorPayload payload{};
    payload.sequence = nextAnchorSequence_++;
    payload.masterTimestampUs = timestampUs;

    medium_.transmit(kMasterNodeId, Packet{
        .dstId = VirtualRFMedium::kBroadcastNodeId,
        .txTimestampUs = timestampUs,
        .payload = rfsync::encodeAnchor(payload),
    });
}

void VirtualSyncMaster::onPacketReceived(const Packet& packet, uint64_t rxTimestampUs) {
    rfsync::FramePayload frame{};
    if (!rfsync::decodeFrame(packet, frame)) {
        return;
    }

    receivedFrames_.push_back(ReceivedFrame{
        packet.srcId,
        rxTimestampUs,
        frame.localTimestampUs,
        frame.estimatedOffsetUs,
        frame.orientation,
        frame.sequence,
    });
    offsetHistory_[packet.srcId].push_back(frame.estimatedOffsetUs);
}

std::vector<VirtualSyncMaster::NodeSyncQuality> VirtualSyncMaster::getSyncQuality() const {
    std::vector<NodeSyncQuality> qualities;
    qualities.reserve(offsetHistory_.size());
    for (const auto& [nodeId, history] : offsetHistory_) {
        qualities.push_back(getSyncQuality(nodeId));
    }
    return qualities;
}

VirtualSyncMaster::NodeSyncQuality VirtualSyncMaster::getSyncQuality(uint8_t nodeId) const {
    NodeSyncQuality quality{};
    quality.nodeId = nodeId;

    const auto histIt = offsetHistory_.find(nodeId);
    if (histIt != offsetHistory_.end()) {
        quality.offsetStdDevUs = computeStdDev(histIt->second);
    }

    for (const auto& frame : receivedFrames_) {
        if (frame.nodeId != nodeId) {
            continue;
        }
        ++quality.framesReceived;
        quality.oneWayLatencyUs += static_cast<float>(
            static_cast<int64_t>(frame.rxTimestampUs) - static_cast<int64_t>(frame.claimedTxTimeUs));
    }

    if (quality.framesReceived > 0u) {
        quality.oneWayLatencyUs /= static_cast<float>(quality.framesReceived);
    }

    return quality;
}

} // namespace sim
