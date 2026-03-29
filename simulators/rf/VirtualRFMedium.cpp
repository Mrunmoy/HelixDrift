#include "VirtualRFMedium.hpp"

#include <algorithm>

namespace sim {

VirtualRFMedium::VirtualRFMedium(const RFMediumConfig& config)
    : config_(config), rng_(42u) {}

void VirtualRFMedium::registerNode(uint8_t nodeId, ReceiveCallback callback) {
    nodes_[nodeId] = std::move(callback);
}

void VirtualRFMedium::unregisterNode(uint8_t nodeId) {
    nodes_.erase(nodeId);
}

void VirtualRFMedium::transmit(uint8_t srcId, const Packet& packet) {
    ++stats_.packetsTransmitted;

    if (shouldDropPacket()) {
        ++stats_.packetsLost;
        return;
    }

    Packet scheduled = packet;
    scheduled.srcId = srcId;
    const uint32_t linkDelayUs = config_.baseLatencyUs + sampleJitterUs();
    inFlight_.push_back(PendingDelivery{
        currentTimeUs_ + linkDelayUs,
        scheduled.txTimestampUs + linkDelayUs,
        std::move(scheduled),
    });
}

void VirtualRFMedium::advanceTimeUs(uint64_t deltaUs) {
    currentTimeUs_ += deltaUs;
    processDeliveries();
}

void VirtualRFMedium::processDeliveries() {
    auto keepBegin = std::remove_if(
        inFlight_.begin(), inFlight_.end(),
        [this](const PendingDelivery& pending) {
            if (pending.deliveryTimeUs > currentTimeUs_) {
                return false;
            }

            if (pending.packet.dstId == kBroadcastNodeId) {
                for (const auto& [nodeId, callback] : nodes_) {
                    if (nodeId == pending.packet.srcId) {
                        continue;
                    }
                    callback(pending.packet, pending.rxTimestampUs);
                    ++stats_.packetsDelivered;
                }
                return true;
            }

            const auto it = nodes_.find(pending.packet.dstId);
            if (it != nodes_.end()) {
                it->second(pending.packet, pending.rxTimestampUs);
                ++stats_.packetsDelivered;
            }
            return true;
        });

    inFlight_.erase(keepBegin, inFlight_.end());
}

bool VirtualRFMedium::shouldDropPacket() {
    if (config_.packetLossRate <= 0.0f) {
        return false;
    }
    if (config_.packetLossRate >= 1.0f) {
        return true;
    }

    std::bernoulli_distribution drop(config_.packetLossRate);
    return drop(rng_);
}

uint32_t VirtualRFMedium::sampleJitterUs() {
    if (config_.jitterMaxUs == 0u) {
        return 0u;
    }

    std::uniform_int_distribution<uint32_t> jitter(0u, config_.jitterMaxUs);
    return jitter(rng_);
}

} // namespace sim
