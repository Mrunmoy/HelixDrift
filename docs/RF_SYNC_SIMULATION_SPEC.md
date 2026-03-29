# RF/Sync Simulation Specification

**Document ID:** SIM-RFSYNC-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (research/review)  
**Target:** Codex implementation team (Milestone 4)  
**Status:** Implementation-Ready

---

## 1. Purpose

Define host-simulation infrastructure for testing RF transport and time synchronization algorithms before hardware availability. This spec enables deterministic validation of:

- Anchor exchange protocols (two-way message, beacon, etc.)
- Clock offset estimation and tracking
- Packet loss recovery
- Multi-node synchronization convergence

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Virtual RF Medium                                    │
│  ┌──────────┐      ┌──────────┐      ┌──────────┐      ┌──────────┐        │
│  │  Node 1  │←────→│  Node 2  │←────→│  Node N  │←────→│  Master  │        │
│  │( harness)│      │( harness)│      │( harness)│      │( anchor) │        │
│  └────┬─────┘      └────┬─────┘      └────┬─────┘      └────┬─────┘        │
│       │                 │                  │                 │              │
│       └─────────────────┴──────────────────┘─────────────────┘              │
│                              Shared Medium                                  │
│                    (configurable latency, loss, jitter)                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Component Specifications

### 3.1 VirtualRFMedium

**Location:** `simulators/rf/VirtualRFMedium.hpp`  
**Responsibility:** Models shared RF channel with configurable impairments

```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <random>

namespace sim {

struct Packet {
    uint8_t srcId;
    uint8_t dstId;  // 0xFF = broadcast
    uint64_t txTimestampUs;   // Sender's local time
    std::vector<uint8_t> payload;
    uint8_t priority;  // For arbitration simulation
};

struct RFMediumConfig {
    // Latency model (one-way)
    uint32_t baseLatencyUs = 500;       // Minimum propagation delay
    uint32_t jitterMaxUs = 100;         // Uniform jitter [0, jitterMax]
    
    // Loss model
    float packetLossRate = 0.0f;        // Bernoulli loss probability
    uint32_t burstLossLength = 0;       // Gilbert-Elliott burst length
    
    // Capture effect (for Nordic ESB simulation)
    bool enableCaptureEffect = true;    // Stronger signal wins
    float captureMarginDb = 6.0f;       // Required margin for capture
    
    // Duty cycle / availability
    float masterDutyCycle = 0.01f;      // Master TX fraction (1% typical)
    uint32_t slotDurationUs = 1000;     // For TDMA simulation
};

class VirtualRFMedium {
public:
    using ReceiveCallback = std::function<void(const Packet& packet, 
                                                uint64_t rxTimestampUs)>;
    
    explicit VirtualRFMedium(const RFMediumConfig& config);
    
    // Node registration
    void registerNode(uint8_t nodeId, ReceiveCallback callback);
    void unregisterNode(uint8_t nodeId);
    
    // Transmit from a node
    void transmit(uint8_t srcId, const Packet& packet);
    
    // Time advancement (required for latency simulation)
    void advanceTimeUs(uint64_t deltaUs);
    uint64_t nowUs() const { return currentTimeUs_; }
    
    // Impairment injection
    void setPacketLossRate(float rate);
    void triggerBurstLoss(uint32_t durationUs);
    
    // Statistics
    struct Stats {
        uint32_t packetsTransmitted;
        uint32_t packetsDelivered;
        uint32_t packetsLost;
        float averageLatencyUs;
    };
    Stats getStats() const;
    void resetStats();

private:
    RFMediumConfig config_;
    uint64_t currentTimeUs_ = 0;
    std::unordered_map<uint8_t, ReceiveCallback> nodes_;
    std::vector<std::pair<uint64_t, Packet>> inFlight_;  // (deliveryTime, packet)
    std::mt19937 rng_;
    Stats stats_;
    
    void processDeliveries();
    bool shouldDropPacket();
};

} // namespace sim
```

### 3.2 VirtualSyncNode

**Location:** `simulators/rf/VirtualSyncNode.hpp`  
**Responsibility:** Simulates a mocap node with local clock drift, wrapping the sensor harness

```cpp
#pragma once

#include "VirtualMocapNodeHarness.hpp"
#include "VirtualRFMedium.hpp"
#include <cstdint>
#include <functional>

namespace sim {

struct ClockModel {
    // Clock error model: t_local = (1 + drift) * t_true + offset
    float driftPpm = 0.0f;           // Parts per million (20 ppm = ±0.002%)
    uint64_t offsetUs = 0;           // Fixed offset at t=0
    float jitterUs = 0.0f;           // Sampling jitter (std dev)
    
    static ClockModel randomCrystal(float maxDriftPpm = 20.0f);
    static ClockModel randomTCXO(float maxDriftPpm = 2.0f);
};

class VirtualSyncNode {
public:
    VirtualSyncNode(uint8_t nodeId, 
                    VirtualRFMedium& medium,
                    const ClockModel& clock,
                    uint32_t frameIntervalUs = 20000);
    
    // Initialization
    bool init();
    
    // Time advancement
    void advanceTimeUs(uint64_t deltaUs);
    
    // Frame generation
    bool tick();  // Returns true if frame emitted
    
    // RF reception (called by medium)
    void onPacketReceived(const Packet& packet, uint64_t rxTimestampUs);
    
    // Anchor transmission (for two-way sync)
    void sendAnchorRequest();
    
    // Statistics
    struct SyncStats {
        uint32_t framesSent;
        uint32_t framesAcked;
        uint32_t anchorsSent;
        uint32_t anchorsReceived;
        float currentOffsetErrorUs;  // Estimated - true offset
    };
    SyncStats getStats() const;
    
    // Ground truth accessors (for test validation)
    uint64_t trueTimeUs() const;  // Perfect reference time
    uint64_t localTimeUs() const; // Drifted local time
    int64_t trueOffsetUs() const; // What offset *should* be

private:
    uint8_t nodeId_;
    VirtualRFMedium& medium_;
    ClockModel clock_;
    
    VirtualMocapNodeHarness harness_;
    VirtualClock trueClock_;  // Ground truth
    uint64_t localTimeUs_ = 0;
    
    // Sync algorithm state (implementer fills in)
    struct SyncState;
    std::unique_ptr<SyncState> sync_;
    
    void transmitFrame(const sf::Quaternion& orientation);
    void handleAnchorResponse(const Packet& packet);
};

} // namespace sim
```

### 3.3 VirtualSyncMaster

**Location:** `simulators/rf/VirtualSyncMaster.hpp`  
**Responsibility:** Simulates master node that broadcasts anchors and receives frames

```cpp
#pragma once

#include "VirtualRFMedium.hpp"
#include <cstdint>
#include <vector>
#include <map>

namespace sim {

struct ReceivedFrame {
    uint8_t nodeId;
    uint64_t rxTimestampUs;      // Master's local time
    uint64_t claimedTxTimeUs;    // What node claims it transmitted
    sf::Quaternion orientation;
    uint32_t sequenceNum;
};

class VirtualSyncMaster {
public:
    explicit VirtualSyncMaster(VirtualRFMedium& medium,
                               uint32_t anchorPeriodUs = 100000);  // 10 Hz
    
    // Time advancement
    void advanceTimeUs(uint64_t deltaUs);
    
    // Anchor broadcast
    void broadcastAnchor();
    
    // RF reception
    void onPacketReceived(const Packet& packet, uint64_t rxTimestampUs);
    
    // Collected frames
    const std::vector<ReceivedFrame>& getReceivedFrames() const;
    void clearFrames();
    
    // Per-node sync quality
    struct NodeSyncQuality {
        uint8_t nodeId;
        float offsetStdDevUs;       // Stability of clock offset
        float oneWayLatencyUs;      // Estimated (RTT/2)
        uint32_t framesReceived;
        uint32_t framesDropped;
    };
    std::vector<NodeSyncQuality> getSyncQuality() const;

private:
    VirtualRFMedium& medium_;
    uint32_t anchorPeriodUs_;
    uint64_t lastAnchorUs_ = 0;
    uint64_t localTimeUs_ = 0;
    
    std::vector<ReceivedFrame> receivedFrames_;
    std::map<uint8_t, std::vector<int64_t>> offsetHistory_;  // For std dev
};

} // namespace sim
```

---

## 4. Test Scenarios

### 4.1 Test: Basic Anchor Exchange

**File:** `simulators/tests/test_rf_sync_basic.cpp`

```cpp
TEST(RFSyncBasic, TwoWayAnchorCalculatesOffset) {
    // Setup
    VirtualRFMedium medium(RFMediumConfig{});
    VirtualSyncMaster master(medium);
    
    ClockModel nodeClock;
    nodeClock.driftPpm = 10.0f;  // +10 ppm = node runs fast
    nodeClock.offsetUs = 5000;   // 5 ms ahead
    
    VirtualSyncNode node(1, medium, nodeClock);
    ASSERT_TRUE(node.init());
    
    // Advance 100ms, send anchor
    medium.advanceTimeUs(100000);
    master.broadcastAnchor();
    medium.advanceTimeUs(10000);  // Let packet propagate
    
    // Node should have estimated offset close to true value
    EXPECT_NEAR(node.getSyncOffsetUs(), 5000, 200);  // ±200µs tolerance
}
```

### 4.2 Test: Clock Drift Tracking

```cpp
TEST(RFSyncBasic, DriftCompensationOverTime) {
    VirtualRFMedium medium(RFMediumConfig{});
    VirtualSyncMaster master(medium);
    
    // Node with 20 ppm drift (worst case for crystal)
    VirtualSyncNode node(1, medium, ClockModel::randomCrystal(20.0f));
    ASSERT_TRUE(node.init());
    
    // Run for 10 seconds with anchors every 100ms
    for (int i = 0; i < 100; ++i) {
        medium.advanceTimeUs(100000);
        master.broadcastAnchor();
        medium.advanceTimeUs(10000);
    }
    
    // Sync quality should show low offset variance
    auto quality = master.getSyncQuality(1);
    EXPECT_LT(quality.offsetStdDevUs, 50.0f);  // < 50µs std dev
}
```

### 4.3 Test: Packet Loss Recovery

```cpp
TEST(RFSyncRobustness, Handles50PercentLoss) {
    RFMediumConfig config;
    config.packetLossRate = 0.5f;  // 50% loss
    
    VirtualRFMedium medium(config);
    VirtualSyncMaster master(medium);
    VirtualSyncNode node(1, medium, ClockModel::randomCrystal());
    ASSERT_TRUE(node.init());
    
    // Run for 5 seconds
    for (int i = 0; i < 50; ++i) {
        medium.advanceTimeUs(100000);
        master.broadcastAnchor();
        medium.advanceTimeUs(10000);
    }
    
    // Should still achieve sync despite loss
    EXPECT_GT(node.getStats().anchorsReceived, 10);  // At least some got through
    EXPECT_LT(node.getSyncOffsetErrorUs(), 1000);     // Within 1ms
}
```

### 4.4 Test: Multi-Node Convergence

```cpp
TEST(RFSyncMultiNode, SixNodesConverge) {
    VirtualRFMedium medium(RFMediumConfig{});
    VirtualSyncMaster master(medium);
    
    std::vector<std::unique_ptr<VirtualSyncNode>> nodes;
    for (int i = 1; i <= 6; ++i) {
        nodes.push_back(std::make_unique<VirtualSyncNode>(
            i, medium, ClockModel::randomCrystal()));
        ASSERT_TRUE(nodes.back()->init());
    }
    
    // Run convergence period
    for (int i = 0; i < 100; ++i) {
        medium.advanceTimeUs(100000);
        master.broadcastAnchor();
        medium.advanceTimeUs(10000);
    }
    
    // All nodes should be synchronized to within 1ms
    auto qualities = master.getSyncQuality();
    EXPECT_EQ(qualities.size(), 6);
    for (const auto& q : qualities) {
        EXPECT_LT(q.offsetStdDevUs, 100.0f);
    }
}
```

---

## 5. Implementation Tasks

| Task | File | Priority | Est. Effort |
|------|------|----------|-------------|
| 1. VirtualRFMedium core | `simulators/rf/VirtualRFMedium.hpp/cpp` | 🔴 High | 4h |
| 2. VirtualSyncNode | `simulators/rf/VirtualSyncNode.hpp/cpp` | 🔴 High | 6h |
| 3. VirtualSyncMaster | `simulators/rf/VirtualSyncMaster.hpp/cpp` | 🔴 High | 4h |
| 4. Basic sync tests | `simulators/tests/test_rf_sync_basic.cpp` | 🔴 High | 3h |
| 5. Robustness tests | `simulators/tests/test_rf_sync_robustness.cpp` | 🟡 Medium | 4h |
| 6. Multi-node tests | `simulators/tests/test_rf_sync_multinode.cpp` | 🟡 Medium | 3h |

**Total Estimated Effort:** ~24 hours

---

## 6. Integration with Existing Harness

The VirtualSyncNode wraps VirtualMocapNodeHarness:

```cpp
VirtualSyncNode::VirtualSyncNode(uint8_t nodeId, VirtualRFMedium& medium,
                                 const ClockModel& clock, uint32_t frameIntervalUs)
    : nodeId_(nodeId)
    , medium_(medium)
    , clock_(clock)
    , harness_(nodeId, frameIntervalUs)  // Existing harness
{
    medium_.registerNode(nodeId, 
        [this](const auto& p, auto t) { onPacketReceived(p, t); });
}

bool VirtualSyncNode::tick() {
    bool emitted = harness_.tick();
    if (emitted) {
        // Get latest sample and transmit
        auto& frames = harness_.captureTransport().frames;
        if (!frames.empty()) {
            transmitFrame(frames.back().orientation);
        }
    }
    return emitted;
}
```

---

## 7. Validation Criteria

| Criterion | Target | Measurement |
|-----------|--------|-------------|
| Sync accuracy | < 1ms offset error | Compare estimated to true offset |
| Drift tracking | < 100µs std dev | Over 60s with 20 ppm drift |
| Loss recovery | < 5s convergence | From 50% loss to < 1ms accuracy |
| Multi-node | < 500µs inter-node | Max offset difference across 6 nodes |

---

## 8. References

- Nordic ESB documentation: `docs/research/nordic_esb_timing.md` (from Kimi's earlier research)
- Original sync research: `docs/research/SYNC_TIMING_RESEARCH.md`
- Existing harness: `simulators/fixtures/VirtualMocapNodeHarness.hpp`
