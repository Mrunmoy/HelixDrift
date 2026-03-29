# Pose Inference Feasibility Matrix

**Status**: Draft v0.1  
**Owner**: Kimi / Pose Feasibility Research  
**Date**: 2026-03-29  

---

## 1. Executive Summary

This document compares approaches for reconstructing human/animal pose from wearable sensors. The goal is to identify what is realistically achievable with HelixDrift's sensor stack (IMU + magnetometer + barometer) and what requires additional hardware.

**Key Finding**: 
- **Best approach for v1**: Orientation-only with kinematic constraints
- **Critical insight**: Position estimation drifts too fast for practical use without external references
- **Recommendation**: Focus on joint angles from relative orientation, defer absolute position

---

## 2. Approaches Compared

### 2.1 Orientation-Only Per Segment

**Description**: Each sensor provides 3D orientation. Skeleton built by propagating orientations through kinematic chain with fixed bone lengths.

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Joint angles, segment orientations, relative pose |
| **Expected accuracy** | 3-5° RMS for orientation, 5-10° for joint angles |
| **Drift** | Slow (magnetometer-aided AHRS), < 10° over 60s |
| **Failure mode** | Magnetic interference affects heading, gyro bias causes slow rotation drift |
| **Complexity** | Low - standard sensor fusion |
| **Hardware needed** | IMU + mag (already in HelixDrift) |
| **Simulation requirements** | Validate orientation accuracy, joint angle recovery |

**Pros**:
- ✅ Works with existing hardware
- ✅ Relatively stable (magnetic north reference)
- ✅ Low computational cost
- ✅ Well-understood problem

**Cons**:
- ❌ No absolute position
- ❌ Root position unknown (except optional height from IK)
- ❌ Magnetic interference in some environments

**Verdict**: ✅ **PRIMARY APPROACH for v1**

---

### 2.2 Orientation + Fixed Bone Lengths

**Description**: Extends orientation-only with known segment lengths. Uses forward kinematics to compute joint positions.

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Everything in 2.1 + joint positions (relative to root) |
| **Expected accuracy** | Position accuracy ~2-5 cm (propagates from orientation error) |
| **Drift** | Same as orientation (slow) |
| **Failure mode** | Bone length errors cause systematic position offsets |
| **Complexity** | Low-Medium - forward kinematics |
| **Hardware needed** | Same as 2.1 + bone length calibration |
| **Simulation requirements** | FK validation, bone length sensitivity |

**Pros**:
- ✅ Provides joint positions for visualization
- ✅ Natural extension of orientation-only
- ✅ Position error bounded (doesn't drift)

**Cons**:
- ⚠️ Requires bone length measurement/calibration
- ⚠️ Position accuracy limited by orientation accuracy
- ❌ Still no global position

**Verdict**: ✅ **IMPLEMENT WITH v1** - Natural extension

---

### 2.3 Orientation + Gait/Contact Constraints

**Description**: Adds biomechanical constraints (feet on ground, joint limits) and gait phase detection to improve pose estimation.

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Everything in 2.2 + root height + ground contact |
| **Expected accuracy** | Root height ~5-10 cm, contact detection > 90% |
| **Drift** | Height bounded by ground contact (resets to zero) |
| **Failure mode** | False contact detection, missed steps |
| **Complexity** | Medium - need contact detection algorithm |
| **Hardware needed** | Same as 2.1 + maybe pressure sensors (optional) |
| **Simulation requirements** | Contact detection validation, height estimation |

**Pros**:
- ✅ Root height becomes bounded (resets each step)
- ✅ More realistic motion (feet don't float)
- ✅ Can detect locomotion state (stand/walk/run)

**Cons**:
- ⚠️ Contact detection can be tricky (accelerometer signatures)
- ⚠️ Only works for lower body / feet
- ⚠️ Upper body still "floating" relative to root

**Verdict**: ⚠️ **OPTIONAL v1 ENHANCEMENT** - Implement if time permits

---

### 2.4 IMU Dead Reckoning (Position from Double Integration)

**Description**: Double-integrate accelerometer data to estimate position. Subtract gravity, integrate to velocity, integrate to position.

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Position, velocity (in theory) |
| **Expected accuracy** | > 1 meter error in < 30 seconds |
| **Drift** | Catastrophic - unbounded, exponential error growth |
| **Failure mode** | Position completely wrong within seconds |
| **Complexity** | Low to implement, impossible to make work well |
| **Hardware needed** | IMU only (but needs very high quality) |
| **Simulation requirements** | Will show why it doesn't work |

**Pros**:
- ✅ In theory gives absolute position
- ✅ No external references needed

**Cons**:
- ❌ **Does not work in practice** - fundamental physics limitation
- ❌ Noise integration causes exponential drift
- ❌ Gravity subtraction errors compound
- ❌ Cannot determine initial velocity

**Verdict**: ❌ **NOT VIABLE** - Do not pursue for v1

---

### 2.5 Barometer-Assisted Height Changes

**Description**: Use barometric pressure to estimate vertical position changes (altitude).

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Relative height changes (vertical motion) |
| **Expected accuracy** | ~10-50 cm per floor, noise ~10 cm |
| **Drift** | Slow (weather changes), but noisy |
| **Failure mode** | Pressure changes from weather, HVAC, doors |
| **Complexity** | Low - simple altitude formula |
| **Hardware needed** | Barometer (already in HelixDrift) |
| **Simulation requirements** | Pressure-to-altitude validation |

**Pros**:
- ✅ Hardware already present (LPS22DF)
- ✅ Can detect floor changes, jumps, crouches

**Cons**:
- ⚠️ Very noisy for small height changes (< 1m)
- ⚠️ Affected by environmental pressure changes
- ❌ Only vertical, no horizontal position

**Verdict**: ⚠️ **SUPPLEMENTARY ONLY** - Use as auxiliary input, not primary

---

### 2.6 RF-Based Ranging (Not Custom RF)

**Description**: Use RSSI or time-of-flight between nodes to estimate inter-node distances.

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Distance between nodes (1D) |
| **Expected accuracy** | RSSI: ±1-2m, ToF: ±10-50 cm (theoretical) |
| **Drift** | RSSI varies with body absorption, orientation |
| **Failure mode** | Multipath, body shadowing, inconsistent readings |
| **Complexity** | Medium - ToF requires precise timing |
| **Hardware needed** | Radios with ToF capability (not standard BLE) |
| **Simulation requirements** | Range validation under various conditions |

**Pros**:
- ✅ Could constrain kinematic chain
- ✅ No external infrastructure

**Cons**:
- ❌ RSSI too noisy for useful ranging on body
- ❌ ToF requires hardware not in current design
- ❌ Body absorbs/shadows 2.4 GHz signals

**Verdict**: ❌ **NOT VIABLE with current hardware** - Would need UWB

---

### 2.7 UWB (Ultra-Wideband) Ranging

**Description**: Use UWB radios for precise time-of-flight distance measurements between nodes or to anchors.

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Precise distances (±10 cm), potentially position via trilateration |
| **Expected accuracy** | 10-30 cm ranging accuracy |
| **Drift** | None - direct measurement each packet |
| **Failure mode** | Line-of-sight occlusion, multipath in cluttered environments |
| **Complexity** | High - UWB stack, anchor infrastructure |
| **Hardware needed** | UWB radios (not in current design) |
| **Simulation requirements** | Range validation, position solving |

**Pros**:
- ✅ Accurate ranging
- ✅ Could provide position constraints
- ✅ Mature technology (Apple U1, Decawave)

**Cons**:
- ❌ **Requires additional hardware** (UWB chip per node)
- ❌ Infrastructure needed (anchors for absolute position)
- ❌ Power consumption higher
- ❌ Cost increase

**Verdict**: ❌ **OUT OF SCOPE for v1** - Consider for v2 if position is critical

---

### 2.8 Vision-Assisted Alignment

**Description**: Use cameras (external or on-body) to correct drift and provide absolute reference.

| Aspect | Assessment |
|--------|------------|
| **What it can estimate** | Absolute position, drift-free orientation |
| **Expected accuracy** | cm to mm depending on camera setup |
| **Drift** | None (if cameras fixed) or slow (if SLAM) |
| **Failure mode** | Occlusion, lighting changes, processing latency |
| **Complexity** | Very High - computer vision, calibration |
| **Hardware needed** | Cameras (external or wearable), compute |
| **Simulation requirements** | Camera modeling, pose estimation |

**Pros**:
- ✅ Can eliminate all drift
- ✅ Absolute reference

**Cons**:
- ❌ **Requires external infrastructure** (cameras in environment)
- ❌ Or heavy on-body compute (wearable cameras + SLAM)
- ❌ Complexity very high
- ❌ Latency from vision processing

**Verdict**: ❌ **OUT OF SCOPE for v1** - Consider for future versions

---

## 3. Comparison Matrix

| Approach | Orientation | Joint Angles | Position | Drift | Hardware | Complexity | v1 Fit |
|----------|:-----------:|:------------:|:--------:|:-----:|:--------:|:----------:|:------:|
| 1. Orientation-only | ✅ | ✅ | ❌ | Slow | Current | Low | ⭐⭐⭐ |
| 2. + Bone lengths | ✅ | ✅ | Relative | Slow | Current | Low | ⭐⭐⭐ |
| 3. + Contact constraints | ✅ | ✅ | Height | Bounded | Current | Med | ⭐⭐ |
| 4. Dead reckoning | ❌ | ❌ | Wrong | Fast | Current | Low | ❌ |
| 5. Barometer assist | ⚠️ | ⚠️ | Vertical | Slow | Current | Low | ⭐ |
| 6. RF ranging | ❌ | ❌ | ❌ | N/A | Different | Med | ❌ |
| 7. UWB | ✅ | ✅ | ✅ | None | Extra | High | v2 |
| 8. Vision | ✅ | ✅ | ✅ | None | Extra | Very High | v2+ |

**Legend**: 
- ⭐⭐⭐ = Primary v1 approach
- ⭐⭐ = Good enhancement
- ⭐ = Supplementary
- ❌ = Not viable

---

## 4. v1 Recommendation

### 4.1 Primary Approach: Orientation + Bone Lengths

**Stack**:
1. Per-node orientation from IMU+mag fusion
2. Forward kinematics with fixed bone lengths
3. Joint angles from relative orientation

**Why this wins**:
- Works with existing hardware
- Bounded error (doesn't drift catastrophically)
- Sufficient for most use cases (VR, animation, biomechanics)
- Implementable within project timeline

### 4.2 Optional Enhancement: Contact Constraints

Add if time permits:
- Ground contact detection from accelerometer patterns
- Root height estimation when standing
- Gait phase detection (stance vs swing)

### 4.3 Explicitly Not in v1

- Global position tracking
- UWB integration
- Vision fusion
- Relative node position estimation

---

## 5. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Magnetic interference | Medium | Heading drift | Detect interference, warn user, fallback to gyro-only |
| Bone length calibration error | Medium | Systematic position offset | Calibration wizard, user adjustment |
| Joint angle ambiguity | Low | Wrong pose | Constrain to anatomical limits, use context |
| Orientation drift in motion | Medium | Accumulated error | Accelerate magnetometer weighting during static periods |
| User expects position | High | Product disappointment | Clear documentation, show use cases where orientation is sufficient |

---

## 6. Simulation Experiments Required

| Experiment | Validates | Success Criteria |
|------------|-----------|------------------|
| Static orientation hold | Basic accuracy | < 5° RMS over 60s |
| Known rotation | Joint angle recovery | Recovered angle within 5° |
| Arm swing | Dynamic tracking | Smooth joint trajectories |
| Magnetic disturbance | Robustness | < 15° error during/after interference |
| Bone length sensitivity | Calibration importance | Document error per cm of mismeasurement |
| Full body chain | Multi-node integration | No kinematic singularities, reasonable pose |

---

## 7. Handoff Summary

### For Codex / Fusion Team

**Implement**:
- Mahony/Madgwick AHRS with mag aiding
- Orientation quality/confidence metrics
- Magnetic interference detection

**Validate**:
- Orientation accuracy < 5° RMS
- Drift < 10° over 60 seconds

### For Future Skeleton Solver Team

**Implement**:
- Forward kinematics from orientations
- Bone length configuration system
- Joint angle extraction (relative rotation)
- Simple IK for ground contact (optional)

**Validate**:
- Joint angles match expected biomechanics
- Position error bounded (doesn't explode)

---

## 8. Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-29 | Initial feasibility matrix - 8 approaches compared |

