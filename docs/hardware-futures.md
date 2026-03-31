# Hardware Futures Exploration

**Status**: Research v0.1  
**Owner**: Kimi / Hardware Futures  
**Date**: 2026-03-29  

---

## 1. Executive Summary

This document explores future hardware directions for HelixDrift beyond the current v1 design (nRF52840 + LSM6DSO + BMM350 + LPS22DF). The goal is to identify opportunities for cost reduction, power savings, performance improvement, and feature expansion.

**Key Findings**:
- **v1 is well-chosen**: nRF52840 provides good headroom for development
- **Cost reduction path**: nRF52833 possible later if memory usage permits
- **Sensor alternatives**: Some newer IMUs offer better power/performance tradeoffs
- **UWB integration**: Would enable position tracking but adds ~$5-10 BOM cost
- **Power optimization**: 50-70% battery life improvement possible with aggressive optimization

---

## 2. MCU Alternatives

### 2.1 Current: nRF52840

**Specs**:
- 1 MB Flash, 256 KB RAM
- Cortex-M4F @ 64 MHz
- Bluetooth 5.2, 802.15.4, proprietary 2.4 GHz
- USB, NFC
- $3.50-4.50 (1k units)

**Assessment**: ✅ **Correct choice for v1** - provides headroom for development

### 2.2 Cost-Down Option: nRF52833

**Specs**:
- 128 KB Flash, 32 KB RAM
- Cortex-M4F @ 64 MHz
- Bluetooth 5.2, 802.15.4, proprietary 2.4 GHz
- No USB, no NFC
- $2.50-3.00 (1k units)

**Trade-offs**:
- ✅ $1-1.50 cheaper per node
- ✅ Smaller footprint
- ❌ Less RAM (32 KB vs 256 KB) - may be tight with full stack
- ❌ No USB (OTA via BLE only)

**When to consider**:
- After v1 firmware stabilizes
- If memory usage < 100 KB flash, < 20 KB RAM
- If USB not needed for target use case

**Recommendation**: ⚠️ **Evaluate after v1 ships** - measure actual resource usage first

### 2.3 Performance Upgrade: nRF5340

**Specs**:
- Dual-core: Application (M33) + Network (M33)
- 1 MB Flash, 512 KB RAM
- Bluetooth 5.3, 802.15.4, Thread, Zigbee, proprietary
- $4.50-5.50 (1k units)

**Trade-offs**:
- ✅ 2x RAM, dual-core for isolation
- ✅ Better security features
- ✅ More headroom for future features
- ❌ More expensive
- ❌ Higher power consumption (two cores)
- ❌ Overkill for v1 scope

**When to consider**:
- v2 with complex protocol requirements
- Need to run multiple radios simultaneously
- Security/crypto requirements increase

**Recommendation**: ❌ **Not for v1** - Only if v1 outgrows nRF52840

### 2.4 Alternative Ecosystem: Archived Secondary MCU Path

**Specs**:
- Dual-core Xtensa LX7 @ 240 MHz
- Wi-Fi + Bluetooth 5.0
- 8 MB Flash typical
- $2.50-3.00

**Trade-offs**:
- ✅ Much faster CPU
- ✅ Wi-Fi for different use cases
- ✅ Lower cost than nRF52840
- ❌ Higher power consumption (Wi-Fi)
- ❌ Different toolchain (not ARM)
- ❌ Not Nordic ecosystem

**Recommendation**: ❌ **Not recommended** - Project is nRF52-first

---

## 3. Sensor Alternatives

### 3.1 IMU: Current LSM6DSO

**Specs**:
- 6-axis (accel + gyro)
- ODR: up to 6.66 kHz (accel), 6.66 kHz (gyro)
- Current: 0.55 mA (high performance), 4 µA (low power)
- $1.50-2.00

### 3.2 IMU Alternative: Bosch BMI270

**Specs**:
- 6-axis optimized for wearables
- Current: 685 µA (full), 8 µA (low power)
- Built-in gesture recognition, activity detection
- Auto low-power modes
- $1.20-1.50

**Trade-offs**:
- ✅ Better power optimization (auto modes)
- ✅ Built-in activity context
- ✅ Slightly cheaper
- ⚠️ Lower max ODR (1.6 kHz) - fine for mocap
- ❌ Different register map (software effort)

**Recommendation**: ⚠️ **Consider for v1.5/v2** - If power optimization critical

### 3.3 IMU Alternative: TDK ICM-42688-P

**Specs**:
- 6-axis, low noise
- ODR: up to 32 kHz
- Current: ~900 µA active
- Very low noise: 2.8 mdps/√Hz gyro
- $1.80-2.20

**Trade-offs**:
- ✅ Lower noise = better orientation
- ✅ Higher ODR for fast motion
- ❌ Higher power consumption
- ❌ More expensive

**Recommendation**: ⚠️ **Consider if orientation accuracy insufficient**

### 3.4 Magnetometer: Current BMM350

**Specs**:
- 3-axis magnetic
- ODR: up to 400 Hz
- Current: 250 µA (normal)
- $0.80-1.00

### 3.5 Magnetometer Alternative: Asahi Kasei AK09940

**Specs**:
- Lower power: 100 µA
- Similar performance
- $0.60-0.80

**Trade-offs**:
- ✅ Lower power
- ✅ Lower cost
- ⚠️ Different interface

**Recommendation**: ⚠️ **Minor benefit** - BMM350 is fine

### 3.6 9-DoF Combo Alternative: TDK ICM-20948

**Specs**:
- Accel + gyro + mag in one package
- On-chip DMP (Digital Motion Processor)
- $2.50-3.00

**Trade-offs**:
- ✅ One chip instead of two
- ✅ On-chip fusion (but we want to control algorithm)
- ❌ DMP is black box
- ❌ More expensive than separate sensors
- ❌ Less flexible

**Recommendation**: ❌ **Not recommended** - We want control of fusion algorithm

---

## 4. Position Sensing Options (v2+)

### 4.1 UWB: Qorvo DW3000

**Specs**:
- IEEE 802.15.4z HRP UWB
- Ranging accuracy: ±10 cm
- Current: ~50 mA TX, ~20 mA RX (bursts)
- $3.00-4.00

**Use case**:
- Inter-node ranging for position constraints
- Anchor-based absolute positioning

**Power impact**:
- High instantaneous current (but short bursts)
- With 10 Hz ranging: ~5-10 mA average
- Reduces battery life significantly

**Cost impact**:
- +$3-4 per node
- +$50-100 for anchor hardware

**Recommendation**: ⚠️ **v2 consideration** - If market demands position tracking

### 4.2 BLE AoA/AoD (Angle of Arrival)

**Specs**:
- Bluetooth 5.1+ feature
- Direction finding via antenna array
- Requires multiple antennas on receiver

**Use case**:
- Position via triangulation
- Lower accuracy than UWB (~1-2m)

**Trade-offs**:
- ✅ Uses existing radio (no extra chip)
- ✅ Lower cost than UWB
- ❌ Requires antenna array design
- ❌ Lower accuracy
- ❌ Complex calibration

**Recommendation**: ⚠️ **Research only** - Unproven for this use case

### 4.3 Pressure-Based Height: Already in v1

**Current**: LPS22DF barometer

**Limitations**:
- Only height changes, not horizontal
- Environmental noise (HVAC, weather)
- ~10-50 cm accuracy per floor

**Enhancement**: Better algorithms for step detection and floor estimation

---

## 5. Power Optimization Opportunities

### 5.1 Current Estimate (v1 Design)

| Component | Active | Sleep | Notes |
|-----------|--------|-------|-------|
| nRF52840 | 5-15 mA | 2-5 µA | Radio dominates |
| LSM6DSO | 550 µA | 4 µA | Always-on for mocap |
| BMM350 | 250 µA | 1 µA | Always-on |
| LPS22DF | 12 µA | 1 µA | Always-on |
| **Total** | **~7-16 mA** | **~10 µA** | |

**Battery life estimate** (150 mAh coin cell):
- Active: 150 mAh / 10 mA = 15 hours
- With 10% duty cycle: ~150 hours
- Realistic with protocol: 4-8 hours

### 5.2 Optimization Strategies

| Strategy | Savings | Complexity | v1 Applicable? |
|----------|---------|------------|----------------|
| **Adaptive ODR** | 30-50% | Low | ✅ Yes |
| **Mag duty cycling** | 10-20% | Medium | ✅ Yes |
| **Radio duty cycle** | 40-60% | Medium | ✅ Yes (protocol design) |
| **Sensor low-power modes** | 20-30% | Low | ✅ Yes |
| **IMU auto-wake** | 50-70% | Medium | ✅ Yes |
| **UWB instead of continuous** | N/A | High | ❌ v2 only |

### 5.3 Adaptive ODR (Most Impact)

**Concept**: Reduce sample rate during slow motion

```
If motion < threshold:
    ODR = 50 Hz
Else:
    ODR = 100-200 Hz
```

**Impact**: 30-50% power savings during rest periods

**Implementation**: IMU has built-in activity detection (BMI270 especially good)

### 5.4 Radio Duty Cycle Optimization

Current design (TDMA at 50 Hz):
- Duty cycle: ~30-40%

Optimized (burst mode):
- Duty cycle: ~10-15%
- Savings: 50-60% radio power

**Implementation**: Protocol change in v1.x

---

## 6. Packaging and Form Factor

### 6.1 Current v1 Target

**Expected size**: 20mm × 15mm × 8mm
**Mounting**: Velcro strap or clip
**Weight**: < 10g per node

### 6.2 Miniaturization Path

| Component | Current | Mini Option | Size Impact |
|-----------|---------|-------------|-------------|
| MCU | QFN48 | QFN32 | -30% |
| IMU | LGA14 | LGA12 | -15% |
| Battery | 150 mAh | 100 mAh | -30% |
| **Total** | ~20×15×8mm | ~15×10×5mm | **-50% volume** |

**Trade-off**: Smaller battery = shorter life (or lower duty cycle)

### 6.3 Alternative Battery Options

| Type | Capacity | Size | Recharge | Cost |
|------|----------|------|----------|------|
| CR2032 | 220 mAh | 20mm×3.2mm | No | $0.30 |
| LiPo 150mAh | 150 mAh | 15×10×5mm | Yes | $2.00 |
| LiPo 100mAh | 100 mAh | 12×8×4mm | Yes | $1.50 |
| Solid state | 200 mAh | 15×10×3mm | Yes | $5.00 (new) |

**Recommendation**: 
- v1: LiPo 150mAh (rechargeable, decent life)
- v2: Consider solid state if available at scale

---

## 7. Cost Roadmap

### 7.1 v1 BOM Estimate

| Component | Cost (1k) |
|-----------|-----------|
| nRF52840 | $4.00 |
| LSM6DSO | $1.75 |
| BMM350 | $0.90 |
| LPS22DF | $0.80 |
| PCB + passives | $1.50 |
| Battery | $2.00 |
| Enclosure | $1.50 |
| **Total** | **~$12-14** |

**Target retail**: $49-79 per node (4-6x BOM typical)

### 7.2 Cost-Reduced v1.5 (if volumes increase)

| Change | Savings |
|--------|---------|
| nRF52833 | -$1.00 |
| BMI270 (vs LSM6DSO) | -$0.30 |
| Optimized enclosure | -$0.50 |
| Volume discounts | -$1.00 |
| **Total savings** | **~$2.80** |
| **New BOM** | **~$10** |

### 7.3 High-Performance v2

| Addition | Cost |
|----------|------|
| UWB chip | +$3.50 |
| Larger battery | +$1.00 |
| Better IMU | +$0.50 |
| **Total** | **~$17-20** |

**Target retail**: $89-129 per node

---

## 8. Recommendations

### 8.1 v1 (Current)

**Keep current design**:
- nRF52840 (good headroom)
- LSM6DSO + BMM350 + LPS22DF (proven, available)

**Minor optimizations**:
- Implement adaptive ODR in software
- Optimize radio duty cycle in protocol

### 8.2 v1.5 (6-12 months after v1)

**If memory usage permits**:
- Evaluate nRF52833 cost-down
- Consider BMI270 for better power
- Smaller enclosure if battery allows

### 8.3 v2 (Future)

**If market demands position**:
- UWB for ranging
- Keep orientation as primary
- Position as "enhancement mode"

**If accuracy needs improvement**:
- TDK ICM-42688-P (lower noise IMU)
- Better magnetometer calibration

---

## 9. Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| nRF52840 shortage | High | Dual-source, consider nRF52833 backup |
| Sensor EOL | Medium | Monitor lifecycle, test alternatives |
| Battery supply | Low | Multiple LiPo suppliers |
| UWB availability | Medium | DW3000 is current gen, stable |

---

## 10. Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-29 | Initial hardware futures exploration |
