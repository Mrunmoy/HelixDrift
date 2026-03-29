# RF/Sync Timing Requirements

**Status**: Draft v0.1  
**Owner**: Kimi / RF-Sync Research  
**Date**: 2026-03-29  

---

## 1. Executive Summary

This document defines the quantitative timing budget for HelixDrift's node-to-master communication. The goal is to derive concrete latency and synchronization targets that will guide protocol selection and system design.

**Key Findings (Preliminary)**:
- Target end-to-end latency: **< 20 ms** for VR/AR applications, **< 50 ms** for animation/film
- Per-node sampling: **50 Hz minimum** (20 ms period), **100-200 Hz preferred** for fast motion
- Transport budget: **< 5-10 ms** one-way to leave headroom for fusion and rendering
- Sync accuracy: **< 1 ms** inter-node skew for multi-node kinematic chains

---

## 2. Use Case Analysis

### 2.1 Primary Use Cases

| Use Case | Latency Sensitivity | Typical Budget | Notes |
|----------|---------------------|----------------|-------|
| VR/AR real-time avatar | **Very High** | < 20 ms end-to-end | Motion-to-photon must beat human perception threshold |
| Film/TV animation | Medium | < 50 ms acceptable | Offline processing possible, but real-time preview preferred |
| Game streaming | High | < 30 ms | Similar to VR but slightly more tolerant |
| Sports biomechanics | Low-Medium | < 100 ms | Analysis often post-hoc, but real-time feedback growing |
| Medical/therapy | Medium | < 50 ms | Real-time feedback for rehabilitation exercises |

### 2.2 Human Perception Thresholds

Research on human perception of motion-to-visual delay:

| Delay | Perceptibility | User Experience |
|-------|----------------|-----------------|
| < 10 ms | Imperceptible | "Instant" - no discomfort |
| 10-20 ms | Barely perceptible | Minor mismatch, generally acceptable for VR |
| 20-50 ms | Noticeable | "Floaty" feeling, reduced presence |
| 50-100 ms | Clearly delayed | Breaks immersion, may cause discomfort |
| > 100 ms | Unusable for VR | Simulator sickness, unacceptable for real-time |

**References**:
- VR latency studies (Oculus, Valve) suggest < 20 ms as target for comfortable VR
- Animation industry typically targets < 50 ms for real-time mocap preview
- IEEE VR standards recommend < 20 ms motion-to-photon for HMDs

---

## 3. System Latency Budget

### 3.1 Component Breakdown

End-to-end latency consists of:

```
[Sensor Sampling] → [Fusion] → [Transport] → [Master Processing] → [Render Output]
```

| Component | Target Latency | Notes |
|-----------|----------------|-------|
| Sensor sampling | 10-20 ms | At 50-100 Hz sample rate |
| Sensor fusion | 5-10 ms | Mahony AHRS + calibration |
| Packetization | 1-2 ms | Frame packing, timestamping |
| **Transport (RF)** | **5-10 ms** | **This is what RF/Sync owns** |
| Master receive processing | 2-5 ms | Decode, timestamp mapping |
| Skeleton solving | 5-15 ms | IK/forward kinematics (if needed) |
| Render output | 8-16 ms | At 60-120 Hz refresh |
| **Total budget** | **36-78 ms** | Must fit use case requirements |

### 3.2 Transport Layer Budget

For the RF/Sync team to hit system targets:

| Scenario | End-to-End Target | Headroom for Transport |
|----------|-------------------|------------------------|
| VR/AR (tight) | 20 ms | ~5 ms one-way / 10 ms round-trip |
| Animation (moderate) | 50 ms | ~15 ms one-way / 30 ms round-trip |

**Conclusion**: Transport layer should target **< 10 ms one-way** to be viable for VR/AR, with **< 5 ms** preferred for comfortable margins.

---

## 4. Synchronization Requirements

### 4.1 Inter-Node Sync

For multi-node body tracking (e.g., arm with shoulder+elbow+wrist):

| Metric | Target | Rationale |
|--------|--------|-----------|
| Inter-node timestamp skew | < 1 ms | Prevents kinematic chain breaking |
| Anchor update interval | 50-100 ms | Balance accuracy vs overhead |
| Sync convergence time | < 500 ms on startup | Fast join for new nodes |
| Drift tolerance | < 2 ms over 60s | Between anchor updates |

### 4.2 Master Timebase Accuracy

| Metric | Target | Notes |
|--------|--------|-------|
| Master clock stability | < 50 ppm | Standard crystal tolerance |
| Node clock stability | < 50 ppm | nRF52 typical |
| Maximum drift rate | ~100 ppm combined | Requires periodic sync |
| Time sync accuracy | < 100 µs | For sub-millisecond node alignment |

---

## 5. Network Impairment Tolerance

### 5.1 Expected Wireless Conditions

On-body wearable scenario:

| Impairment | Expected Range | System Must Handle |
|------------|----------------|-------------------|
| Packet loss | 1-5% typical, up to 20% in congestion | Graceful degradation |
| Latency variation (jitter) | ±2-5 ms | Buffer or tolerate |
| Burst loss | Up to 3-5 consecutive packets | Interpolation/extrapolation |
| Reordering | Rare but possible | Handle or drop |

### 5.2 Degradation Modes

When impairments exceed design limits:

| Severity | System Behavior |
|----------|-----------------|
| Minor (loss < 5%) | Interpolate missing frames, maintain sync |
| Moderate (loss 5-20%) | Reduce update rate, extrapolate, flag quality |
| Severe (loss > 20%) | Freeze pose, wait for recovery, alert user |

---

## 6. Derived Requirements

### 6.1 Protocol Requirements

Based on the above analysis:

| Requirement | Value | Priority |
|-------------|-------|----------|
| One-way latency | < 10 ms (target < 5 ms) | Must |
| Packet overhead | < 20 bytes | Should |
| Update cadence | 50-100 Hz per node | Must |
| Sync anchor interval | 50-100 ms | Should |
| Packet loss tolerance | Up to 20% with graceful degradation | Must |
| Multi-node support | At least 6 nodes simultaneously | Must |

### 6.2 MCU Resource Budget (nRF52)

| Resource | Budget | Notes |
|----------|--------|-------|
| Radio active time | < 5% duty cycle for battery | Protocol must be efficient |
| CPU for protocol stack | < 10% of CPU | Leave headroom for fusion |
| RAM for buffering | < 2 KB | Per-node tx/rx buffers |
| Code size | < 8 KB flash | Protocol implementation |

---

## 7. Open Questions

1. **Fusion latency**: What is actual measured time for SensorFusion pipeline? Need data from Codex/Fusion team.

2. **Multi-node scaling**: Does transport latency increase with node count? Need investigation of TDMA vs contention-based schemes.

3. **Anchor reliability**: How many anchors can be lost before sync degrades? Need simulation testing.

4. **Body location impact**: Does node position on body affect RF performance (absorption, shadowing)? Need characterization.

---

## 8. Next Steps

1. ✅ **Q1 Complete**: Timing budget defined (this document)
2. **Q2 In Progress**: Compare BLE vs proprietary 2.4 GHz options against these requirements
3. **Q3 Pending**: Design sync architecture that meets < 1 ms inter-node skew target

---

## References

- Oculus VR Latency Requirements (internal industry standards)
- IEEE VR Standards Committee recommendations
- IEEE 1588 PTP (Precision Time Protocol) - lightweight variants
- Nordic Semiconductor nRF52 product specifications
- Ant+ protocol timing specifications (reference for low-latency sensor networks)

---

**Document History**

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-29 | Initial draft - Q1 timing budget analysis |

