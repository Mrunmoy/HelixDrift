#pragma once

#include "RFSyncProtocol.hpp"
#include "VirtualRFMedium.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace sim {

struct ReceivedFrame {
    uint8_t nodeId = 0;
    uint64_t rxTimestampUs = 0;
    uint64_t claimedTxTimeUs = 0;
    sf::Quaternion orientation{};
    uint32_t sequenceNum = 0;
};

class VirtualSyncMaster {
public:
    struct NodeSyncQuality {
        uint8_t nodeId = 0;
        float offsetStdDevUs = 0.0f;
        float oneWayLatencyUs = 0.0f;
        uint32_t framesReceived = 0;
        uint32_t framesDropped = 0;
    };

    explicit VirtualSyncMaster(VirtualRFMedium& medium,
                               uint32_t anchorPeriodUs = 100000);

    void advanceTimeUs(uint64_t deltaUs);
    void broadcastAnchor();
    void onPacketReceived(const Packet& packet, uint64_t rxTimestampUs);

    const std::vector<ReceivedFrame>& getReceivedFrames() const { return receivedFrames_; }
    void clearFrames() { receivedFrames_.clear(); }

    std::vector<NodeSyncQuality> getSyncQuality() const;
    NodeSyncQuality getSyncQuality(uint8_t nodeId) const;
    uint64_t nowUs() const { return localTimeUs_; }

private:
    static constexpr uint8_t kMasterNodeId = 0;

    VirtualRFMedium& medium_;
    uint32_t anchorPeriodUs_ = 100000;
    uint64_t localTimeUs_ = 0;
    uint32_t nextAnchorSequence_ = 0;

    std::vector<ReceivedFrame> receivedFrames_;
    std::map<uint8_t, std::vector<int64_t>> offsetHistory_;
};

} // namespace sim
