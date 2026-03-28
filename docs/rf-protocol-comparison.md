# RF Protocol Comparison for HelixDrift

**Status**: Draft v0.1  
**Owner**: Kimi / RF-Sync Research  
**Date**: 2026-03-29  

---

## 1. Executive Summary

This document compares viable low-latency communication options for nRF52-class MCUs in the HelixDrift wearable mocap application. The goal is to identify which protocol approaches can meet the timing requirements established in `rf-sync-requirements.md`.

**Key Requirements (from rf-sync-requirements.md)**:
- One-way latency: < 10 ms (target < 5 ms)
- Update cadence: 50-100 Hz per node
- Multi-node: At least 6 nodes simultaneously
- Packet loss tolerance: Up to 20% with graceful degradation
- Radio duty cycle: < 5% for battery life

**Candidates Evaluated**:
1. Bluetooth LE (BLE) - Standard and 5.2 Isochronous
2. Proprietary 2.4 GHz (Nordic ESB/Gazell)
3. 802.15.4 (Thread/Zigbee)
4. BLE + Custom Timeslot Hybrid

---

## 2. Option 1: Bluetooth LE (Standard)

### 2.1 Overview
Standard BLE connection-oriented mode using SoftDevice on nRF52.

### 2.2 Performance Characteristics

| Metric | Typical Value | Meets Requirement? |
|--------|---------------|-------------------|
| Connection interval | 7.5 ms - 4 s | ⚠️ Marginal at 7.5 ms |
| Latency (1 packet) | 5-15 ms | ⚠️ Borderline |
| Latency (with slave latency) | 20-100 ms | ❌ Too slow |
| Throughput | ~1 Mbps theoretical | ✅ Adequate |
| Multi-node (simultaneous) | ~4-8 connections | ⚠️ Marginal |
| Duty cycle | 5-15% | ❌ Too high for battery |

### 2.3 Analysis

**Pros**:
- Mature stack (SoftDevice)
- Ecosystem compatibility (phones, tablets)
- Built-in pairing/security

**Cons**:
- Connection interval is the bottleneck
- 7.5 ms minimum interval gives ~15-20 ms round-trip
- Multi-central architecture limits node count
- High duty cycle for low-latency operation

### 2.4 Verdict

❌ **Not suitable** for VR/AR latency targets (< 20 ms end-to-end). May work for animation use cases with < 50 ms tolerance if battery life is not critical.

---

## 3. Option 2: BLE 5.2 Isochronous Channels (LE Audio)

### 3.1 Overview
BLE 5.2 adds Isochronous Channels designed for audio streaming with low latency and tight synchronization.

### 3.2 Performance Characteristics

| Metric | Typical Value | Meets Requirement? |
|--------|---------------|-------------------|
| Isochronous interval | 5-10 ms typical | ✅ Good |
| Latency (BIS) | 10-20 ms | ⚠️ Borderline |
| Latency (CIS) | 5-15 ms | ⚠️ Borderline |
| Synchronization | < 10 µs between receivers | ✅ Excellent |
| Multi-node | 1 broadcaster + many observers | ✅ Good |

### 3.3 Analysis

**Pros**:
- Designed for exactly this use case (synced streaming)
- Excellent inter-node synchronization (< 10 µs)
- Standard protocol, growing ecosystem

**Cons**:
- Requires BLE 5.2 capable hardware (nRF52833/840 have it)
- Complex stack implementation
- Still limited by BLE framing overhead
- May require SoftDevice updates for stable support

### 3.4 Verdict

⚠️ **Promising but complex**. The synchronization features are perfect, but latency is still marginal. Worth investigating if Nordic's implementation can achieve sub-10 ms reliably.

---

## 4. Option 3: Proprietary 2.4 GHz (Nordic ESB/Gazell)

### 4.1 Overview
Nordic's Enhanced ShockBurst (ESB) or Gazell protocol - custom 2.4 GHz using Nordic's proprietary stack.

### 4.2 Performance Characteristics

| Metric | Typical Value | Meets Requirement? |
|--------|---------------|-------------------|
| Packet TX time | ~100-300 µs | ✅ Excellent |
| Round-trip latency | 0.5-2 ms | ✅ Excellent |
| One-way latency | < 1 ms typical | ✅ Excellent |
| Multi-node (star) | 1 master + 8+ devices | ✅ Good |
| Duty cycle | < 1% possible | ✅ Excellent for battery |
| Packet size | 1-252 bytes | ✅ Flexible |

### 4.3 Analysis

**Pros**:
- Lowest latency option (sub-millisecond achievable)
- Minimal overhead, very efficient
- Simple protocol, small code footprint
- Excellent battery life with low duty cycle
- Proven in gaming peripherals (mice, keyboards)

**Cons**:
- Custom protocol - no phone/tablet compatibility
- Must implement own sync/timing logic
- No built-in security (must add if needed)
- Ecosystem lock-in to Nordic

### 4.4 Verdict

✅ **Strong candidate**. Meets all timing requirements comfortably. Best option for pure latency and battery. Trade-off is custom implementation effort.

---

## 5. Option 4: 802.15.4 (Thread/Zigbee)

### 5.1 Overview
Mesh networking protocols based on IEEE 802.15.4.

### 5.2 Performance Characteristics

| Metric | Typical Value | Meets Requirement? |
|--------|---------------|-------------------|
| Latency (single hop) | 10-50 ms | ❌ Too slow |
| Latency (multi-hop mesh) | 50-200 ms | ❌ Much too slow |
| Throughput | 250 kbps | ⚠️ Marginal |
| Multi-node | 100s of devices | ✅ Excellent |
| Duty cycle | 1-5% | ✅ Good |

### 5.3 Analysis

**Pros**:
- Excellent multi-node scaling
- Mesh networking for range extension
- Open standards

**Cons**:
- Latency is too high for real-time mocap
- Designed for sensor networks, not streaming
- Complex stack (Thread especially)

### 5.4 Verdict

❌ **Not suitable**. Latency fundamentally incompatible with < 20 ms VR targets.

---

## 6. Option 5: BLE + Custom Timeslot Hybrid

### 6.1 Overview
Use Nordic's SoftDevice Radio Timeslot API to inject custom protocol during BLE timeslots.

### 6.2 Performance Characteristics

| Metric | Typical Value | Meets Requirement? |
|--------|---------------|-------------------|
| Custom timeslot duration | 1-10 ms | ✅ Flexible |
| Latency (custom slot) | Similar to ESB | ✅ Excellent |
| BLE coexistence | Maintained | ✅ Good |
| Implementation complexity | High | ❌ Challenging |

### 6.3 Analysis

**Pros**:
- Keep BLE for configuration/OTA
- Use custom protocol for data streaming
- Best of both worlds for ecosystem

**Cons**:
- Complex implementation
- SoftDevice timing constraints
- Timeslot scheduling complexity
- Debugging difficulty

### 6.4 Verdict

⚠️ **Advanced option**. Good for production if team has expertise, but high risk for initial development.

---

## 7. Comparison Matrix

| Criteria | Weight | BLE Standard | BLE 5.2 Isoch. | Proprietary 2.4G | 802.15.4 | BLE+Timeslot |
|----------|--------|--------------|----------------|------------------|----------|--------------|
| **Latency** | High | ❌ | ⚠️ | ✅ | ❌ | ✅ |
| **Multi-node** | High | ⚠️ | ✅ | ✅ | ✅ | ✅ |
| **Battery life** | High | ❌ | ⚠️ | ✅ | ✅ | ✅ |
| **Implementation risk** | Medium | ✅ | ⚠️ | ✅ | ❌ | ❌ |
| **Ecosystem** | Medium | ✅ | ✅ | ❌ | ⚠️ | ✅ |
| **Security** | Medium | ✅ | ✅ | ⚠️ | ✅ | ✅ |

**Legend**: ✅ = Good, ⚠️ = Marginal, ❌ = Poor

---

## 8. Recommendations

### 8.1 Primary Recommendation: Proprietary 2.4 GHz (Nordic ESB)

**Rationale**:
1. **Latency**: Sub-millisecond round-trip easily meets < 5 ms transport budget
2. **Battery**: < 1% duty cycle enables all-day wearable use
3. **Multi-node**: Star topology supports 6+ nodes with simple TDMA
4. **Proven**: Battle-tested in gaming peripherals with similar latency requirements

**Implementation path**:
- Start with Nordic's ESB library examples
- Implement simple TDMA schedule (master polls nodes sequentially)
- Add timestamps to each packet for sync
- Validate latency with logic analyzer/scope

### 8.2 Alternative: BLE 5.2 Isochronous

**When to consider**:
- If ecosystem compatibility (phones) is mandatory
- If team has BLE 5.2 expertise
- For later product versions when stack matures

### 8.3 Hybrid for Production

Long-term, consider BLE + Timeslot hybrid:
- BLE for device management, OTA updates, phone connectivity
- Custom 2.4 GHz for real-time mocap streaming
- Switch between modes based on use case

---

## 9. Open Questions for Q3

1. **TDMA design**: What slot allocation strategy minimizes latency for 6 nodes at 50-100 Hz?

2. **Sync method**: Should master poll (deterministic) or nodes contend (flexible)?

3. **Clock sync**: How to achieve < 1 ms inter-node skew without complex PTP?

4. **Packet format**: What is minimal overhead for quaternion + timestamp + health?

---

## 10. Next Steps

1. ✅ **Q2 Complete**: Protocol comparison done (this document)
2. **Q3 In Progress**: Design sync architecture using proprietary 2.4 GHz as baseline
3. **Pending**: Hand off to Codex for implementation harness

---

**Document History**

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-29 | Initial comparison of 5 protocol options |

