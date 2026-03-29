# RF/Sync Architecture Design

**Status**: Draft v0.1  
**Owner**: Kimi / RF-Sync Research  
**Date**: 2026-03-29  

---

## 1. Executive Summary

This document proposes a concrete synchronization and communication architecture for HelixDrift based on:
- **Timing requirements**: `rf-sync-requirements.md` (Q1)
- **Protocol selection**: `rf-protocol-comparison.md` (Q2) - proprietary 2.4 GHz selected

**Proposed Architecture**:
- **Physical layer**: Nordic Proprietary 2.4 GHz (ESB/Gazell)
- **Topology**: Star (1 master, N nodes, N ≤ 8)
- **Access method**: TDMA (Time Division Multiple Access)
- **Sync method**: Master-driven anchor broadcasts
- **Target performance**: < 5 ms one-way, < 1 ms inter-node skew

---

## 2. Timing Contract

### 2.1 Time Domains

| Domain | Description | Characteristics |
|--------|-------------|-----------------|
| **Node Local Time (t_node)** | Free-running counter on each node | ~50 ppm drift, wraps at 32-bit |
| **Master Time (t_master)** | Reference timebase at master | Single source of truth |
| **Sample Time (t_sample)** | When sensor data was captured | t_node at capture moment |
| **Transmit Time (t_tx)** | When packet leaves node radio | t_node + processing delay |
| **Receive Time (t_rx)** | When packet arrives at master | t_master at receive moment |

### 2.2 Timestamp Mapping

Goal: Express all events in master time domain.

```
t_master = t_node + offset_node + drift_correction(t)
```

Where:
- `offset_node`: Fixed offset discovered during sync
- `drift_correction(t)`: Linear correction for clock drift (~50 ppm)

### 2.3 Anchor Semantics

**Anchor**: A periodic beacon from master containing master timestamp.

| Field | Size | Purpose |
|-------|------|---------|
| anchor_id | 8 bits | Sequence number for anchor tracking |
| t_master_anchor | 32 bits | Master timestamp at anchor TX |
| slot_assignments | 8-16 bits | Which node owns which TDMA slot |

**Usage**:
- Nodes receive anchor and record `t_node_anchor` (local time)
- Calculate offset: `offset = t_master_anchor - t_node_anchor`
- Apply drift correction over time until next anchor

---

## 3. Protocol Design

### 3.1 Frame Types

| Type | Code | Direction | Purpose |
|------|------|-----------|---------|
| **ANCHOR** | 0xA0 | Master → All | Sync beacon + slot assignments |
| **DATA** | 0xD0 | Node → Master | Mocap sample (quaternion + meta) |
| **ACK** | 0xA1 | Master → Node | Confirmation (optional piggyback) |
| **HEALTH** | 0xH0 | Bidirectional | Status, battery, errors |
| **CONTROL** | 0xC0 | Master → Node | Config, calibrate, sleep |

### 3.2 TDMA Frame Structure

Superframe structure (repeating every 20 ms for 50 Hz):

```
|<-------------------- 20 ms Superframe -------------------->|
|                                                             |
[Anchor][Guard][Slot 0][Guard][Slot 1][Guard]...[Slot N][Guard][Idle]...
|       |     |      |     |      |          |      |      |
|       |     |      |     |      |          |      |      +-- Variable
|       |     |      |     |      |          |      +-- 150 µs
|       |     |      |     |      |          +-- 1 ms (data)
|       |     |      |     |      +-- 150 µs guard
|       |     |      |     +-- 1 ms (data)
|       |     |      +-- 150 µs guard
|       |     +-- 1 ms (node 0 data)
|       +-- 150 µs guard (RX/TX turnaround)
+-- 200 µs (anchor transmission)
```

**Slot allocation for 6 nodes at 50 Hz**:
- Anchor: 200 µs
- Guard after anchor: 150 µs
- 6 data slots: 6 × 1 ms = 6 ms
- 6 guards: 6 × 150 µs = 900 µs
- Total active: ~7.3 ms
- Duty cycle: 7.3/20 = **36.5%** (too high!)

**Optimization for battery**:
- Reduce to 50 Hz anchor, 100 Hz data (burst mode)
- Anchor every 20 ms, nodes send 2 samples per superframe
- Or use shorter superframe: 10 ms for 100 Hz

### 3.3 Optimized 100 Hz Configuration

Superframe: 10 ms (100 Hz)
- Anchor: 200 µs (every 10th frame = 10 Hz anchors)
- 6 slots: 6 × 800 µs = 4.8 ms
- Guards: 7 × 100 µs = 700 µs
- Total: ~5.7 ms active per 10 ms
- Duty cycle: **57%** (still high for 100 Hz)

**Better approach: Dynamic TDMA**
- Nodes only send when they have new data
- Master polls nodes that have data pending
- Reduces duty cycle for low-motion periods

### 3.4 Packet Formats

#### ANCHOR Packet (Master → Broadcast)

```
[0]     Frame type (0xA0)
[1]     Anchor sequence number
[2-5]   t_master_anchor (uint32, microseconds)
[6]     Node mask (which nodes are active)
[7-13]  Slot assignments (1 byte per node = slot index)
[14-15] CRC16
```
**Total: 16 bytes**

#### DATA Packet (Node → Master)

```
[0]     Frame type (0xD0)
[1]     Node ID
[2-5]   t_sample (uint32, node local time at capture)
[6-9]   Quaternion W (int16 fixed point)
[10-11] Quaternion X (int16 fixed point)
[12-13] Quaternion Y (int16 fixed point)
[14-15] Quaternion Z (int16 fixed point)
[16]    Flags (calibration status, health)
[17]    RSSI (measured at node)
[18-19] CRC16
```
**Total: 20 bytes**

**Quaternion encoding**: Q15 fixed-point (1.0 = 32767, range -1.0 to +1.0)
- Precision: ~0.00003 (~0.002 degrees for normalized quaternion)
- Sufficient for mocap

---

## 4. Synchronization Algorithm

### 4.1 Startup Sync (Node Join)

1. **Listen**: Node listens for ANCHOR packets
2. **Capture**: On first ANCHOR, record:
   - `t_master_anchor` from packet
   - `t_node_anchor` from local counter
3. **Calculate initial offset**:
   ```
   offset = t_master_anchor - t_node_anchor
   ```
4. **Wait for slot assignment**: Master includes node in next ANCHOR
5. **Begin transmission**: Send first DATA packet in assigned slot

### 4.2 Continuous Sync

Each ANCHOR reception updates sync:

```
// At anchor n:
dt_anchor = t_node_anchor[n] - t_node_anchor[n-1]  // Local time elapsed
dt_master = t_master_anchor[n] - t_master_anchor[n-1]  // Master time elapsed

// Calculate drift
drift_ppm = (dt_anchor - dt_master) / dt_master * 1e6

// Update offset (smoothed)
offset_new = t_master_anchor[n] - t_node_anchor[n]
offset_filtered = 0.8 * offset_old + 0.2 * offset_new  // Low-pass filter

// Calculate drift rate for interpolation
drift_rate = (offset_new - offset_old) / dt_anchor
```

### 4.3 Timestamp Conversion

When sending DATA packet:
```
t_sample (in packet) = local_time_at_capture  // Node time
```

At master, convert to master time:
```
t_sample_master = t_sample + offset_filtered + drift_rate * (t_now - t_anchor_last)
```

### 4.4 Expected Accuracy

| Factor | Error Contribution |
|--------|-------------------|
| Anchor interval (10-100 ms) | < 0.5 µs quantization |
| Clock drift (50 ppm) | ~5 µs over 100 ms |
| Radio jitter | ~10-50 µs |
| Processing delay | ~100 µs (calibrated out) |
| **Total skew** | **< 200 µs typical** |

**Result**: Easily meets < 1 ms inter-node skew requirement.

---

## 5. Error Handling

### 5.1 Lost ANCHOR

| Consecutive Losses | Action |
|-------------------|--------|
| 1 | Continue with extrapolated drift |
| 2 | Widen TX window (guard time) |
| 3 | Reduce data rate, flag degraded mode |
| 5+ | Return to listen mode, search for anchors |

### 5.2 Lost DATA Packet

Master side:
- Interpolate between last two valid samples
- Extrapolate if gap > 2 samples
- Flag "estimated" quality bit

Node side:
- If ACK not received (if using ACKs), retry in next assigned slot
- If persistent failure, request slot reassignment

### 5.3 Clock Drift Exceeds Threshold

If measured drift > 100 ppm:
- Flag crystal/oscillator issue
- Reduce anchor interval temporarily
- Alert host for maintenance

---

## 6. Host Simulation Plan

### 6.1 Components to Simulate

Per `SIMULATION_BACKLOG.md` Milestone 4:

1. **VirtualMasterClock**: Generates anchor timestamps
2. **VirtualNodeClocks**: Each node with independent drift (±50 ppm)
3. **TDMAScheduler**: Manages slot allocation
4. **SyncFilter**: Implements Section 4.2 algorithm
5. **ImpairmentModel**: Packet loss, jitter, delay

### 6.2 Test Scenarios

| Scenario | Purpose | Success Criteria |
|----------|---------|------------------|
| Nominal sync | Baseline performance | < 200 µs skew |
| Anchor loss | Degradation handling | < 2 ms skew after 5 lost anchors |
| Drift variation | Clock change adaptation | < 500 µs during 100 ppm drift |
| Multi-node join | Startup behavior | All 6 nodes synced within 500 ms |
| Burst loss | Resilience | Graceful degradation, no crashes |

### 6.3 Metrics to Report

```
- Mean/max sync skew per node
- Anchor loss rate
- Sync convergence time
- Drift estimation accuracy
- Timestamp mapping error distribution
```

---

## 7. Implementation Recommendations

### 7.1 Phase 1: Host Simulation (Immediate)

**Owner**: Codex / RF And Sync team (when assigned)

Implement in `simulators/`:
- `VirtualMasterClock.hpp/cpp`
- `VirtualNodeClock.hpp/cpp`
- `TDMAScheduler.hpp/cpp`
- `SyncFilter.hpp/cpp`

Test harness in `tests/test_rf_sync.cpp`:
- Validate sync algorithm with simulated drift
- Test TDMA scheduling fairness
- Measure skew under various conditions

### 7.2 Phase 2: Protocol Validation (Next)

**Owner**: Codex

- Implement packet format encode/decode
- Add CRC validation
- Test multi-node simulation (6 virtual nodes)
- Validate timing with logic analyzer traces

### 7.3 Phase 3: nRF52 Implementation (Later)

**Owner**: Codex / nRF52 Platform team

- Port to Nordic ESB library
- Calibrate radio delays
- Optimize power consumption
- Validate with real hardware

---

## 8. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| TDMA complexity too high | Medium | Implementation slip | Start with fixed schedule, add dynamic later |
| 50 ppm drift insufficient | Low | Sync accuracy | Measure real hardware, adjust filter if needed |
| Multi-node interference | Medium | Packet loss | Implement FEC or retransmission |
| ESB library limitations | Low | Blocked | Fallback to raw radio, or use Gazell |
| Power consumption too high | Medium | Battery life | Optimize duty cycle, reduce update rate |

---

## 9. Handoff to Implementation

### 9.1 Deliverables for Codex

1. ✅ Timing requirements (`rf-sync-requirements.md`)
2. ✅ Protocol selection (`rf-protocol-comparison.md`)
3. ✅ Architecture design (this document)

### 9.2 Recommended Implementation Order

1. VirtualMasterClock + VirtualNodeClock (host sim)
2. SyncFilter algorithm (host sim)
3. TDMAScheduler (host sim)
4. Packet format encode/decode
5. Multi-node integration test
6. Impairment model (latency, jitter, loss)
7. Validation metrics and reporting
8. nRF52 ESB port

### 9.3 Interface Contracts

**SyncFilter API** (proposed):
```cpp
class SyncFilter {
public:
    void onAnchorReceived(uint32_t t_master_anchor, uint32_t t_node_anchor);
    uint32_t nodeToMasterTime(uint32_t t_node) const;
    float getCurrentDriftPpm() const;
    bool isSynced() const;
};
```

**TDMAScheduler API** (proposed):
```cpp
class TDMAScheduler {
public:
    void assignSlot(uint8_t node_id, uint8_t slot_index);
    uint8_t getCurrentSlot(uint32_t t_master) const;
    uint32_t getSlotStartTime(uint8_t slot_index) const;
};
```

---

## 10. Open Questions

1. **Dynamic slot allocation**: Should master adjust slot assignments based on node activity?

2. **Acknowledgment policy**: Explicit ACK per packet vs implicit (next anchor)?

3. **Security**: Is encryption needed for mocap data? (Probably not for v1)

4. **Range testing**: What is actual ESB range with body-worn nodes?

5. **Coexistence**: How does this coexist with BLE for OTA/config?

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-29 | Initial architecture with TDMA, sync algorithm, packet formats |

