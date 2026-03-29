# RF Slice 1 Starter Pack: VirtualRFMedium Core

**Document ID:** KIMI-RF-S1-STARTER-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (implementation-support)  
**Target:** Codex RF And Sync Team (deferred until M2 closes)  
**Effort:** 2-3 hours  
**Risk:** Low (new files only, zero M2 impact)

---

## Overview

This starter pack provides everything Codex needs to implement VirtualRFMedium Core when M2 closes. It is designed to be:
- **100% additive** (new directory `simulators/rf/`, no existing file changes)
- **Zero M2 risk** (does not touch any files Codex is currently working on)
- **Self-contained** (no dependencies on harness code)

---

## Exact File List

### New Files (4 total)
```
simulators/rf/
├── VirtualRFMedium.hpp      # Interface definition
├── VirtualRFMedium.cpp      # Implementation
└── tests/
    └── test_rf_medium_basic.cpp  # First 3 tests (TDD)
```

### CMakeLists.txt Addition
Add to `CMakeLists.txt` in the `helix_simulators` library section:
```cmake
# simulators/rf/CMakeLists.txt or add to main CMakeLists.txt
simulators/rf/VirtualRFMedium.cpp
```

And to `helix_integration_tests`:
```cmake
simulators/tests/test_rf_medium_basic.cpp
```

---

## Interface Sketch

### VirtualRFMedium.hpp
```cpp
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <random>
#include <unordered_map>

namespace sim {

struct Packet {
    uint8_t srcId;
    uint8_t dstId;  // 0xFF = broadcast
    uint64_t txTimestampUs;
    std::vector<uint8_t> payload;
};

struct RFMediumConfig {
    uint32_t baseLatencyUs = 500;       // Minimum propagation delay
    uint32_t jitterMaxUs = 0;           // Uniform jitter [0, jitterMax]
    float packetLossRate = 0.0f;        // Bernoulli loss probability [0,1]
};

class VirtualRFMedium {
public:
    using ReceiveCallback = std::function<void(const Packet& packet, 
                                                uint64_t rxTimestampUs)>;
    
    explicit VirtualRFMedium(const RFMediumConfig& config);
    
    // Node registration (nodes are identified by 8-bit ID)
    void registerNode(uint8_t nodeId, ReceiveCallback callback);
    void unregisterNode(uint8_t nodeId);
    
    // Transmit from a node
    void transmit(uint8_t srcId, const Packet& packet);
    
    // Time advancement (required for latency simulation)
    void advanceTimeUs(uint64_t deltaUs);
    uint64_t nowUs() const { return currentTimeUs_; }
    
    // Statistics
    struct Stats {
        uint32_t packetsTransmitted = 0;
        uint32_t packetsDelivered = 0;
        uint32_t packetsLost = 0;
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

### VirtualRFMedium.cpp (Implementation Notes)
```cpp
#include "VirtualRFMedium.hpp"

namespace sim {

VirtualRFMedium::VirtualRFMedium(const RFMediumConfig& config)
    : config_(config)
    , rng_(42)  // Fixed seed for determinism
{}

void VirtualRFMedium::registerNode(uint8_t nodeId, ReceiveCallback callback) {
    nodes_[nodeId] = callback;
}

void VirtualRFMedium::unregisterNode(uint8_t nodeId) {
    nodes_.erase(nodeId);
}

void VirtualRFMedium::transmit(uint8_t srcId, const Packet& packet) {
    stats_.packetsTransmitted++;
    
    if (shouldDropPacket()) {
        stats_.packetsLost++;
        return;
    }
    
    // Calculate delivery time: now + baseLatency + jitter
    uint32_t jitter = (config_.jitterMaxUs > 0) 
        ? (rng_() % config_.jitterMaxUs) 
        : 0;
    uint64_t deliveryTime = currentTimeUs_ + config_.baseLatencyUs + jitter;
    
    inFlight_.push_back({deliveryTime, packet});
}

void VirtualRFMedium::advanceTimeUs(uint64_t deltaUs) {
    uint64_t targetTime = currentTimeUs_ + deltaUs;
    
    // Process any deliveries that should occur during this time advance
    processDeliveries(targetTime);
    
    currentTimeUs_ = targetTime;
}

void VirtualRFMedium::processDeliveries(uint64_t upToTime) {
    // Iterate through inFlight_, deliver any with deliveryTime <= upToTime
    // For broadcast (dstId == 0xFF), deliver to all registered nodes
    // For unicast, deliver only to matching node
    // Erase delivered packets from inFlight_
}

bool VirtualRFMedium::shouldDropPacket() {
    if (config_.packetLossRate <= 0.0f) return false;
    if (config_.packetLossRate >= 1.0f) return true;
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng_) < config_.packetLossRate;
}

Stats VirtualRFMedium::getStats() const {
    return stats_;
}

void VirtualRFMedium::resetStats() {
    stats_ = Stats{};
}

} // namespace sim
```

---

## Test File Skeleton

### test_rf_medium_basic.cpp
```cpp
#include <gtest/gtest.h>
#include "VirtualRFMedium.hpp"

using namespace sim;

// Test 1: Basic latency
TEST(VirtualRFMedium, SinglePacketDeliveredWithLatency) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    
    bool received = false;
    Packet receivedPacket;
    uint64_t receivedTime = 0;
    
    medium.registerNode(1, [&](const Packet& p, uint64_t t) {
        received = true;
        receivedPacket = p;
        receivedTime = t;
    });
    
    Packet p{.srcId = 2, .dstId = 1, .txTimestampUs = 1000, .payload = {1, 2, 3}};
    medium.transmit(2, p);
    
    // Before latency period: not yet delivered
    EXPECT_FALSE(received);
    
    // Advance exactly to latency boundary
    medium.advanceTimeUs(500);
    
    // Now delivered
    EXPECT_TRUE(received);
    EXPECT_EQ(receivedPacket.srcId, 2);
    EXPECT_EQ(receivedPacket.dstId, 1);
    EXPECT_EQ(receivedPacket.payload.size(), 3);
    EXPECT_EQ(receivedTime, 1500);  // txTime(1000) + latency(500)
}

// Test 2: Broadcast to multiple nodes
TEST(VirtualRFMedium, BroadcastPacketDeliveredToAllNodes) {
    VirtualRFMedium medium({.baseLatencyUs = 100});
    
    bool node1Received = false;
    bool node2Received = false;
    bool node3Received = false;
    
    medium.registerNode(1, [&](const Packet&, uint64_t) { node1Received = true; });
    medium.registerNode(2, [&](const Packet&, uint64_t) { node2Received = true; });
    medium.registerNode(3, [&](const Packet&, uint64_t) { node3Received = true; });
    
    // Broadcast to 0xFF
    Packet p{.srcId = 9, .dstId = 0xFF, .txTimestampUs = 0, .payload = {0xAB}};
    medium.transmit(9, p);
    
    medium.advanceTimeUs(100);
    
    EXPECT_TRUE(node1Received);
    EXPECT_TRUE(node2Received);
    EXPECT_TRUE(node3Received);
}

// Test 3: Packet loss at configured rate
TEST(VirtualRFMedium, PacketLossAtConfiguredRate) {
    // 50% loss rate
    VirtualRFMedium medium({.baseLatencyUs = 100, .packetLossRate = 0.5f});
    
    int receivedCount = 0;
    medium.registerNode(1, [&](const Packet&, uint64_t) { receivedCount++; });
    
    // Transmit 100 packets
    for (int i = 0; i < 100; i++) {
        Packet p{.srcId = 2, .dstId = 1, .txTimestampUs = 0, .payload = {}};
        medium.transmit(2, p);
        medium.advanceTimeUs(100);
    }
    
    // Should receive roughly 50 (within ±15 for statistical variance)
    EXPECT_GE(receivedCount, 35);
    EXPECT_LE(receivedCount, 65);
    
    // Stats should match
    auto stats = medium.getStats();
    EXPECT_EQ(stats.packetsTransmitted, 100);
    EXPECT_EQ(stats.packetsDelivered, receivedCount);
    EXPECT_EQ(stats.packetsLost, 100 - receivedCount);
}
```

---

## First 3 Tests in Execution Order

### Test 1: SinglePacketDeliveredWithLatency (15 min)
**Purpose:** Prove basic latency mechanism works  
**Setup:** Register 1 node, transmit 1 packet, advance time  
**Assert:** Packet received exactly at txTime + baseLatency  
**Why first:** Validates core timing mechanism

### Test 2: BroadcastPacketDeliveredToAllNodes (10 min)
**Purpose:** Prove broadcast (dstId=0xFF) reaches all nodes  
**Setup:** Register 3 nodes, broadcast 1 packet  
**Assert:** All 3 callbacks invoked  
**Why second:** Builds on Test 1, adds routing logic

### Test 3: PacketLossAtConfiguredRate (20 min)
**Purpose:** Prove Bernoulli loss model works statistically  
**Setup:** 50% loss rate, transmit 100 packets  
**Assert:** ~50 received (within tolerance), stats match  
**Why third:** Requires RNG setup, validates impairment model

---

## What NOT to Implement Yet

### Explicitly Out of Scope for Slice 1
- ❌ **Clock drift** (Slice 2: VirtualSyncNode adds this)
- ❌ **Sync algorithms** (Slice 3: VirtualSyncMaster adds this)
- ❌ **Frame/packet integration** (Slice 2+)
- ❌ **Capture effect** (signal strength modeling - future)
- ❌ **Burst loss** (Gilbert-Elliott model - Slice 4)
- ❌ **Out-of-order delivery** (Slice 4)
- ❌ **TDMA slot enforcement** (future optimization)

### Do Not Touch These Files
- ❌ `VirtualMocapNodeHarness.hpp/cpp` (Claude owns)
- ❌ `VirtualSensorAssembly.hpp/cpp` (Codex currently using for M2)
- ❌ `VirtualGimbal.hpp/cpp` (M2 active)
- ❌ `Bmm350Simulator.hpp/cpp` (M2 active)
- ❌ Any existing test files (add new file only)

---

## Anti-Scope-Creep Checklist

If during implementation you find yourself wanting to:
- [ ] Add clock drift → STOP, that's Slice 2
- [ ] Add anchor protocol → STOP, that's Slice 3
- [ ] Modify existing harness code → STOP, ask Claude
- [ ] Add TDMA slots → STOP, future optimization
- [ ] Make it template/generic → STOP, YAGNI for now

---

## Readiness Assessment: Can Codex Start This Later?

| Criterion | Status |
|-----------|--------|
| Additive only (no file modifications) | ✅ Yes |
| New directory (no conflicts with M2) | ✅ Yes |
| No dependencies on harness code | ✅ Yes |
| Self-contained tests | ✅ Yes |
| Low risk of breaking 220+ tests | ✅ Yes |
| Clear completion criteria | ✅ Yes |

**Verdict:** This slice is ready for deferred implementation. Codex can pick it up immediately after M2 closes without any M2 destabilization risk.

---

## Post-Implementation Next Steps

After this slice is implemented and tests pass:
1. Update `simulators/docs/DEV_JOURNAL.md`
2. Request Kimi review
3. Request Claude review
4. Then proceed to **RF Slice 2: ClockModel + VirtualSyncNode** (depends on this slice)

---

**Status:** ✅ Starter pack complete - ready for deferred implementation
