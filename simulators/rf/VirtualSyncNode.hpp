#pragma once

#include "ClockModel.hpp"
#include "RFSyncProtocol.hpp"
#include "VirtualMocapNodeHarness.hpp"
#include "VirtualRFMedium.hpp"

#include <cstdint>

namespace sim {

class VirtualSyncNode {
public:
    struct SyncStats {
        uint32_t framesSent = 0;
        uint32_t framesAcked = 0;
        uint32_t anchorsSent = 0;
        uint32_t anchorsReceived = 0;
        float currentOffsetErrorUs = 0.0f;
    };

    VirtualSyncNode(uint8_t nodeId,
                    VirtualRFMedium& medium,
                    const ClockModel& clock,
                    uint32_t frameIntervalUs = 20000);

    bool init();
    void advanceTimeUs(uint64_t deltaUs);
    bool tick();

    void onPacketReceived(const Packet& packet, uint64_t rxTimestampUs);
    void sendAnchorRequest();

    SyncStats getStats() const;
    uint64_t trueTimeUs() const { return trueTimeUs_; }
    uint64_t localTimeUs() const { return localTimeUs_; }
    int64_t trueOffsetUs() const { return clock_.offsetAtTrueTimeUs(trueTimeUs_); }
    int64_t getSyncOffsetUs() const { return estimatedOffsetUs_; }
    int64_t getSyncOffsetErrorUs() const { return estimatedOffsetUs_ - trueOffsetUs(); }
    uint64_t lastRxTimestampLocalUs() const { return lastRxTimestampLocalUs_; }

private:
    static constexpr uint8_t kMasterNodeId = 0;

    void transmitFrame(const sf::Quaternion& orientation);

    uint8_t nodeId_ = 0;
    VirtualRFMedium& medium_;
    ClockModel clock_{};
    VirtualMocapNodeHarness harness_;
    uint64_t trueTimeUs_ = 0;
    uint64_t localTimeUs_ = 0;
    uint64_t lastRxTimestampLocalUs_ = 0;
    int64_t estimatedOffsetUs_ = 0;
    uint32_t nextFrameSequence_ = 0;
    SyncStats stats_{};
};

} // namespace sim
