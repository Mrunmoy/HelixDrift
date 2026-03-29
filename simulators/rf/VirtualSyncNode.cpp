#include "VirtualSyncNode.hpp"

namespace sim {

VirtualSyncNode::VirtualSyncNode(uint8_t nodeId,
                                 VirtualRFMedium& medium,
                                 const ClockModel& clock,
                                 uint32_t frameIntervalUs)
    : nodeId_(nodeId),
      medium_(medium),
      clock_(clock),
      harness_(nodeId, frameIntervalUs),
      localTimeUs_(clock.mapTrueToLocalUs(0)) {
    medium_.registerNode(nodeId_, [this](const Packet& packet, uint64_t rxTimestampUs) {
        onPacketReceived(packet, rxTimestampUs);
    });
}

bool VirtualSyncNode::init() {
    const bool ok = harness_.initAll();
    if (ok) {
        harness_.resetAndSync();
    }
    return ok;
}

void VirtualSyncNode::advanceTimeUs(uint64_t deltaUs) {
    trueTimeUs_ += deltaUs;
    localTimeUs_ = clock_.mapTrueToLocalUs(trueTimeUs_);
    harness_.advanceTimeUs(deltaUs);
}

bool VirtualSyncNode::tick() {
    const size_t before = harness_.captureTransport().frames.size();
    if (!harness_.tick()) {
        return false;
    }

    if (harness_.captureTransport().frames.size() == before) {
        return false;
    }

    transmitFrame(harness_.captureTransport().frames.back().orientation);
    return true;
}

void VirtualSyncNode::onPacketReceived(const Packet& packet, uint64_t rxTimestampUs) {
    lastRxTimestampLocalUs_ = clock_.mapTrueToLocalUs(rxTimestampUs);

    rfsync::AnchorPayload anchor{};
    if (rfsync::decodeAnchor(packet, anchor)) {
        ++stats_.anchorsReceived;
        estimatedOffsetUs_ = static_cast<int64_t>(lastRxTimestampLocalUs_) -
                             static_cast<int64_t>(rxTimestampUs);
        stats_.currentOffsetErrorUs =
            static_cast<float>(estimatedOffsetUs_ - clock_.offsetAtTrueTimeUs(rxTimestampUs));
    }
}

void VirtualSyncNode::sendAnchorRequest() {
    ++stats_.anchorsSent;
    medium_.transmit(nodeId_, Packet{
        .dstId = kMasterNodeId,
        .txTimestampUs = localTimeUs_,
        .payload = {},
    });
}

VirtualSyncNode::SyncStats VirtualSyncNode::getStats() const {
    return stats_;
}

void VirtualSyncNode::transmitFrame(const sf::Quaternion& orientation) {
    rfsync::FramePayload payload{};
    payload.sequence = nextFrameSequence_++;
    payload.localTimestampUs = localTimeUs_;
    payload.estimatedOffsetUs = estimatedOffsetUs_;
    payload.orientation = orientation;

    medium_.transmit(nodeId_, Packet{
        .dstId = kMasterNodeId,
        .txTimestampUs = localTimeUs_,
        .payload = rfsync::encodeFrame(payload),
    });
    ++stats_.framesSent;
}

} // namespace sim
