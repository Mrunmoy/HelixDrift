#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <unordered_map>
#include <vector>

namespace sim {

struct Packet {
    uint8_t srcId = 0;
    uint8_t dstId = 0;
    uint64_t txTimestampUs = 0;
    std::vector<uint8_t> payload;
    uint8_t priority = 0;
};

struct RFMediumConfig {
    uint32_t baseLatencyUs = 500;
    uint32_t jitterMaxUs = 0;
    float packetLossRate = 0.0f;
};

class VirtualRFMedium {
public:
    static constexpr uint8_t kBroadcastNodeId = 0xFF;

    using ReceiveCallback = std::function<void(const Packet&, uint64_t rxTimestampUs)>;

    struct Stats {
        uint32_t packetsTransmitted = 0;
        uint32_t packetsDelivered = 0;
        uint32_t packetsLost = 0;
    };

    explicit VirtualRFMedium(const RFMediumConfig& config);

    void registerNode(uint8_t nodeId, ReceiveCallback callback);
    void unregisterNode(uint8_t nodeId);

    void transmit(uint8_t srcId, const Packet& packet);

    void advanceTimeUs(uint64_t deltaUs);
    uint64_t nowUs() const { return currentTimeUs_; }
    void triggerBurstLoss(uint64_t durationUs);
    void setPacketLossRate(float rate) { config_.packetLossRate = rate; }

    Stats getStats() const { return stats_; }
    void resetStats() { stats_ = Stats{}; }

private:
    struct PendingDelivery {
        uint64_t deliveryTimeUs = 0;
        uint64_t rxTimestampUs = 0;
        Packet packet{};
    };

    RFMediumConfig config_{};
    uint64_t currentTimeUs_ = 0;
    std::unordered_map<uint8_t, ReceiveCallback> nodes_;
    std::vector<PendingDelivery> inFlight_;
    std::mt19937 rng_;
    Stats stats_{};
    uint64_t burstLossUntilUs_ = 0;

    void processDeliveries();
    bool shouldDropPacket();
    uint32_t sampleJitterUs();
};

} // namespace sim
