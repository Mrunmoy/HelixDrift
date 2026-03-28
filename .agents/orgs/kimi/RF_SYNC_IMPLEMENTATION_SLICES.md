# RF/Sync Implementation Slices

**Document ID:** KIMI-RF-SLICES-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (implementation-support)  
**Target:** Codex RF And Sync Team  
**Source Spec:** `docs/RF_SYNC_SIMULATION_SPEC.md`

---

## Executive Summary

The RF/Sync simulation spec has been decomposed into 6 implementation slices, ranked by value-to-cost ratio and dependency order. Slices 1-3 form the **minimum viable RF/sync harness** and can be implemented independently of current Codex work. Slices 4-6 extend robustness and multi-node capabilities.

---

## Slice 1: VirtualRFMedium Core (Foundation)

**Value/Cost Ratio:** 🔴 Highest (unblocks all other RF work)

### Scope
A shared RF channel with configurable latency and packet loss. No clock drift yet—just packet transport.

### Files
- `simulators/rf/VirtualRFMedium.hpp`
- `simulators/rf/VirtualRFMedium.cpp`

### First Tests
```cpp
// test_rf_medium_basic.cpp
TEST(VirtualRFMedium, SinglePacketDeliveredWithLatency) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    
    bool received = false;
    medium.registerNode(1, [&](const Packet&, uint64_t) { received = true; });
    
    Packet p{.srcId = 2, .dstId = 1, .payload = {1, 2, 3}};
    medium.transmit(2, p);
    
    EXPECT_FALSE(received);  // Not yet delivered
    medium.advanceTimeUs(500);
    EXPECT_TRUE(received);   // Now delivered
}

TEST(VirtualRFMedium, BroadcastPacketDeliveredToAllNodes) {
    // Register 3 nodes, broadcast to 0xFF, all receive after latency
}

TEST(VirtualRFMedium, PacketLossAtConfiguredRate) {
    // 50% loss rate, transmit 100 packets, expect ~50 delivered
}
```

### Measurable Outputs
- [ ] Single-packet latency test passes (deterministic)
- [ ] Broadcast reaches all registered nodes
- [ ] Packet loss rate within ±10% of configured value (statistical)
- [ ] Stats counters accurate (tx/rx counts match)

### Explicitly Out of Scope
- ❌ Clock drift (Slice 2)
- ❌ Sync algorithms (Slice 3)
- ❌ Multi-node frame transmission (Slice 5)
- ❌ Capture effects or signal strength modeling
- ❌ TDMA slot enforcement (future optimization)

### Dependencies
- None (pure new code, no existing harness dependencies)

### Est. Effort
2-3 hours

---

## Slice 2: ClockModel and VirtualSyncNode (Time)

**Value/Cost Ratio:** 🔴 High (enables sync algorithm testing)

### Scope
A virtual node that wraps the existing `VirtualMocapNodeHarness` but adds local clock drift. Can send/receive packets through `VirtualRFMedium` using its drifted local time.

### Files
- `simulators/rf/ClockModel.hpp` (simple struct + factory methods)
- `simulators/rf/VirtualSyncNode.hpp`
- `simulators/rf/VirtualSyncNode.cpp`

### First Tests
```cpp
// test_sync_node_basic.cpp
TEST(VirtualSyncNode, LocalTimeDriftsFromTrueTime) {
    VirtualRFMedium medium({});
    ClockModel clock{.driftPpm = 1000.0f};  // 0.1% fast
    
    VirtualSyncNode node(1, medium, clock);
    node.advanceTimeUs(1000000);  // 1 second true time
    
    // Local time should be ~1.001 seconds
    EXPECT_NEAR(node.localTimeUs(), 1001000, 100);
}

TEST(VirtualSyncNode, TransmitsWithLocalTimestamp) {
    // Node transmits, packet contains node's local time (not true time)
}

TEST(VirtualSyncNode, ReceivesAndRecordsRxTimestamp) {
    // Packet arrives, node records reception in its local time
}
```

### Measurable Outputs
- [ ] Clock drift accumulates correctly over 1-10 seconds
- [ ] Packets transmitted with local timestamps
- [ ] Packets received with local reception timestamps
- [ ] `trueOffsetUs()` returns accurate ground truth for test validation

### Explicitly Out of Scope
- ❌ Sync offset estimation (Slice 3)
- ❌ Frame generation from harness (full integration)
- ❌ Anchor request/response protocol

### Dependencies
- Requires Slice 1 (VirtualRFMedium)
- Reads from `VirtualMocapNodeHarness` but doesn't modify it

### Est. Effort
3-4 hours

---

## Slice 3: VirtualSyncMaster and Basic Anchor Exchange

**Value/Cost Ratio:** 🔴 High (completes minimal sync loop)

### Scope
A master node that broadcasts anchors (timestamp beacons). Nodes estimate clock offset from anchor reception. No drift tracking yet—just single-shot offset estimation.

### Files
- `simulators/rf/VirtualSyncMaster.hpp`
- `simulators/rf/VirtualSyncMaster.cpp`
- `simulators/rf/SimpleOffsetEstimator.hpp` (minimal algorithm)

### First Tests
```cpp
// test_anchor_exchange.cpp
TEST(AnchorExchange, NodeEstimatesOffsetFromSingleAnchor) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    VirtualSyncMaster master(medium);
    
    // Node clock is 5 ms ahead of master
    VirtualSyncNode node(1, medium, {.offsetUs = 5000});
    
    master.broadcastAnchor();  // Contains master timestamp
    medium.advanceTimeUs(1000);  // Let it propagate
    
    // Node should estimate offset close to 5 ms
    EXPECT_NEAR(node.getSyncOffsetUs(), 5000, 500);  // ±500µs tolerance
}

TEST(AnchorExchange, EstimatedOffsetAccountsForLatency) {
    // With 500µs one-way latency, node should still estimate true offset
}
```

### Measurable Outputs
- [ ] Single anchor yields offset estimate within ±1 ms of true value
- [ ] Master receives node's frames with correct timestamp mapping
- [ ] Stats track anchors sent/received per node

### Explicitly Out of Scope
- ❌ Drift tracking over time (future optimization)
- ❌ Two-way message exchange (not needed for TDMA broadcast)
- ❌ Multi-node convergence validation (Slice 5)
- ❌ Packet loss recovery (Slice 4)

### Dependencies
- Requires Slices 1-2

### Est. Effort
3-4 hours

---

## Slice 4: Packet Loss and Robustness

**Value/Cost Ratio:** 🟡 Medium (hardens the sync system)

### Scope
Handle missing anchors, maintain sync during loss bursts, degrade gracefully.

### Files
- Extend existing files above
- `simulators/tests/test_rf_sync_robustness.cpp`

### First Tests
```cpp
// test_rf_sync_robustness.cpp
TEST(Robustness, SyncDegradesGracefullyWith50PercentLoss) {
    VirtualRFMedium medium({.packetLossRate = 0.5f});
    VirtualSyncMaster master(medium);
    VirtualSyncNode node(1, medium, {});
    
    // Run for 100 anchors at 10 Hz (10 seconds)
    for (int i = 0; i < 100; i++) {
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }
    
    // Should have received ~50 anchors
    EXPECT_GT(node.getStats().anchorsReceived, 30);
    // Should still have sync (maybe degraded)
    EXPECT_LT(node.getSyncOffsetErrorUs(), 5000);  // < 5ms error
}

TEST(Robustness, RecoversAfterBurstLoss) {
    // 2-second blackout, then recovery
}
```

### Measurable Outputs
- [ ] Sync maintained at < 5ms error with 50% packet loss
- [ ] Recovery within 5 seconds after 2-second blackout
- [ ] Frame transmission continues during anchor loss

### Explicitly Out of Scope
- ❌ Burst loss modeling (Gilbert-Elliott) - Bernoulli only
- ❌ Adaptive anchor rate
- ❌ Multiple simultaneous node loss patterns

### Dependencies
- Requires Slices 1-3

### Est. Effort
2-3 hours

---

## Slice 5: Multi-Node Convergence

**Value/Cost Ratio:** 🟡 Medium (validates scalability claims)

### Scope
6 nodes with independent clocks, all converging to master timebase. Measure inter-node skew.

### Files
- `simulators/tests/test_rf_sync_multinode.cpp`

### First Tests
```cpp
// test_rf_sync_multinode.cpp
TEST(MultiNode, SixNodesConvergeWithin1msSkew) {
    VirtualRFMedium medium({});
    VirtualSyncMaster master(medium);
    
    std::vector<std::unique_ptr<VirtualSyncNode>> nodes;
    for (int i = 1; i <= 6; i++) {
        nodes.push_back(std::make_unique<VirtualSyncNode>(
            i, medium, ClockModel::randomCrystal()));
    }
    
    // Run 10 seconds of anchors
    for (int i = 0; i < 100; i++) {
        master.broadcastAnchor();
        medium.advanceTimeUs(100000);
    }
    
    // All nodes should agree within 1ms
    auto qualities = master.getSyncQuality();
    for (const auto& q : qualities) {
        EXPECT_LT(q.offsetStdDevUs, 1000.0f);  // < 1ms std dev
    }
}
```

### Measurable Outputs
- [ ] 6 nodes converge to < 1ms inter-node skew
- [ ] Each node achieves < 2ms offset error vs master
- [ ] Frame collection from all nodes at master

### Explicitly Out of Scope
- ❌ Dynamic slot allocation
- ❌ Node join/leave during operation
- ❌ Collision detection/handling (assumes TDMA)

### Dependencies
- Requires Slices 1-4

### Est. Effort
2-3 hours

---

## Slice 6: Drift Tracking (Optional Enhancement)

**Value/Cost Ratio:** 🟢 Lower (optimization, not required for v1)

### Scope
Track clock drift rate over time, predict future offset, reduce anchor frequency.

### Files
- Extend `SimpleOffsetEstimator` with linear regression
- `simulators/tests/test_rf_sync_drift.cpp`

### First Tests
```cpp
TEST(DriftTracking, EstimatesDriftRateOverTime) {
    // 20 ppm drift, track over 10 seconds
    // Should estimate drift within ±5 ppm
}
```

### Measurable Outputs
- [ ] Drift rate estimated within ±5 ppm over 10 seconds
- [ ] Anchor frequency reducible to 1 Hz with maintained accuracy

### Explicitly Out of Scope
- ❌ Temperature compensation
- ❌ Adaptive filtering (Kalman)
- ❌ Multi-path or network topology changes

### Dependencies
- Requires Slices 1-5

### Est. Effort
3-4 hours (can defer to post-MVP)

---

## Ranking Summary

| Slice | Value/Cost | Risk | Can Start Now? | Effort |
|-------|------------|------|----------------|--------|
| 1. VirtualRFMedium | 🔴 Highest | Low | ✅ Yes | 2-3h |
| 2. ClockModel + Node | 🔴 High | Low | ✅ Yes | 3-4h |
| 3. Master + Anchors | 🔴 High | Low | After 1-2 | 3-4h |
| 4. Loss Robustness | 🟡 Medium | Medium | After 1-3 | 2-3h |
| 5. Multi-Node | 🟡 Medium | Medium | After 1-4 | 2-3h |
| 6. Drift Tracking | 🟢 Lower | Low | After 1-5 | 3-4h |

**Recommended First Codex Slice:** Slice 1 (VirtualRFMedium Core)

---

## No-Overlap Confirmation

Per `.agents/OWNERSHIP_MATRIX.md`:
- ✅ This is `simulators/` work → Codex owned
- ✅ Kimi writes spec only, does not implement
- ✅ Claude handles architecture sequencing (not these implementation slices)
- ✅ No file conflicts with current Codex work (new `simulators/rf/` directory)

## Anti-Scope-Creep Checklist

For each slice, if Codex encounters:
- [ ] Need to modify `VirtualMocapNodeHarness` → STOP, ask Kimi/Claude
- [ ] Need to modify `VirtualSensorAssembly` → STOP, ask Kimi/Claude
- [ ] Need to add I2C timing → OUT OF SCOPE, document for future
- [ ] Need real crypto/security → OUT OF SCOPE for v1
- [ ] Need Bluetooth integration → OUT OF SCOPE for v1
