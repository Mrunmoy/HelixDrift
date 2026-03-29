# Pose Inference v1 Recommendation

**Status**: Final Recommendation v1.0  
**Owner**: Kimi / Pose Feasibility Research  
**Date**: 2026-03-29  

---

## 1. Executive Summary

This document provides concrete recommendations for HelixDrift v1 based on:
- `docs/pose-inference-requirements.md` (Q1 - what outputs are needed)
- `docs/pose-inference-feasibility.md` (Q2 - what approaches work)

**Primary Recommendation**:
- **v1 Scope**: Orientation-only mocap with kinematic skeleton fitting
- **Key Insight**: Position tracking drifts catastrophically; orientation is stable and sufficient
- **User Promise**: "Wear 3-6 nodes, see your body move, joint angles accurate to ±10°"
- **Explicit Limitation**: "Global position not tracked - skeleton moves with you"

---

## 2. v1 Product Definition

### 2.1 What v1 DOES

**Core Functionality**:
1. **Stream orientations** from 3-6 body-worn nodes
2. **Compute joint angles** from relative orientations
3. **Display articulated skeleton** in real-time (< 50 ms latency)
4. **Provide quality metrics** (confidence, drift warnings)

**Use Cases Supported**:
- ✅ VR/AR avatar animation
- ✅ Film/TV motion capture (preview and final)
- ✅ Sports biomechanics analysis
- ✅ Game input and gesture recognition
- ✅ Fitness/training form feedback

### 2.2 What v1 DOES NOT DO

**Explicitly Out of Scope**:
1. ❌ Track global position in room (walk around, see on map)
2. ❌ Precise foot placement (feet may appear to slide)
3. ❌ Track without magnetometer (gyro-only mode not supported)
4. ❌ Work through walls (mag interference warning only)
5. ❌ Track multiple people simultaneously

**Why These Are Deferred**:
- Global position requires UWB or vision (extra hardware)
- Foot placement needs contact detection (optional enhancement)
- Gyro-only accumulates rotation drift too fast
- Multi-person needs protocol changes and more bandwidth

### 2.3 Success Criteria

| Metric | Target | Measurement |
|--------|--------|-------------|
| Orientation accuracy | < 5° RMS | Comparison to ground truth gimbal |
| Joint angle accuracy | < 10° | Expected from orientation error propagation |
| Latency | < 50 ms | Motion-to-display time |
| Drift rate | < 10°/min | Max orientation drift during motion |
| Nodes supported | 3-6 | Simultaneous tracking |
| Battery life | > 4 hours | With 50-100 Hz update rate |

---

## 3. Technical Architecture

### 3.1 Data Flow

```
[Node 1-6] --(orientations)--> [Master] --(skeleton)--> [Display]
     |                              |
     v                              v
[IMU+Mag Fusion]          [Skeleton Solver]
- Mahony AHRS             - Forward kinematics
- 50-100 Hz               - Bone length model
- Quaternion output       - Joint angle extraction
```

### 3.2 Node Placement (Recommended)

**Minimum Viable (3 nodes)**:
- Node 1: Torso (root)
- Node 2: Upper arm
- Node 3: Lower arm
- *Tracks: One arm, torso orientation*

**Basic Body (5 nodes)**:
- Node 1: Pelvis (root)
- Node 2: Thigh
- Node 3: Shin
- Node 4: Torso
- Node 5: Upper arm
- *Tracks: One leg, torso, one arm*

**Full Upper Body (6 nodes)**:
- Node 1: Chest (root)
- Node 2: Left upper arm
- Node 3: Left lower arm
- Node 4: Right upper arm
- Node 5: Right lower arm
- Node 6: Head (optional)
- *Tracks: Both arms, torso, head*

**Full Body (6+ nodes)**:
- Add: Pelvis, thighs, shins
- *Tracks: Full biped locomotion*

### 3.3 Skeleton Model

**Simplified Human Skeleton**:
```
Root (pelvis or chest)
├── Spine → Head
│   └── Neck
├── Left Arm Chain
│   ├── Shoulder (ball joint)
│   ├── Upper arm
│   ├── Elbow (hinge)
│   ├── Lower arm
│   └── Wrist (optional)
├── Right Arm Chain
│   └── [same as left]
├── Left Leg Chain
│   ├── Hip (ball joint)
│   ├── Thigh
│   ├── Knee (hinge)
│   ├── Shin
│   └── Ankle (optional)
└── Right Leg Chain
    └── [same as left]
```

**Constraints**:
- Hinge joints: 1 DOF (elbow, knee)
- Ball joints: 3 DOF (shoulder, hip)
- Spinal joints: Simplified as rigid or limited DOF

---

## 4. Implementation Phases

### 4.1 Phase 1: Single Node Orientation (Baseline)

**Deliverables**:
- One-node streaming to master
- Orientation visualization (rotating cube or arrow)
- Quality metrics displayed

**Validation**:
- Static accuracy test
- Rotation tracking test
- Drift measurement over 60s

**Owner**: Codex / Sensor Fusion

### 4.2 Phase 2: Two-Node Joint Angle

**Deliverables**:
- Two nodes streaming
- Joint angle computation
- Simple 2-segment visualization

**Validation**:
- Known angle test (e.g., elbow flexion)
- Accuracy vs ground truth

**Owner**: Codex / Fusion and Skeleton

### 4.3 Phase 3: Multi-Node Skeleton

**Deliverables**:
- 3-6 node support
- Full skeleton solver
- Forward kinematics
- Bone length configuration

**Validation**:
- Biomechanical pose tests
- Natural motion validation
- Drift tracking across chain

**Owner**: Codex / Fusion and Skeleton

### 4.4 Phase 4: Quality Enhancements (Optional)

**Deliverables**:
- Magnetic interference detection
- Drift warnings
- Contact-based root height (if doing leg tracking)
- Calibration wizard

**Validation**:
- Interference robustness
- User calibration accuracy

**Owner**: Codex / Host Tools

---

## 5. Simulation Experiments (Detailed Plan)

### 5.1 Experiment 1: Static Orientation Accuracy

**Purpose**: Establish baseline orientation performance

**Setup**:
- Virtual gimbal holds fixed pose
- Node stationary for 60 seconds
- Record orientation output

**Metrics**:
- Mean orientation error (vs ground truth)
- RMS error
- Max error
- Drift rate (deg/minute)

**Success Criteria**:
- RMS error < 5°
- Drift < 10° over 60s

**Owner**: Codex / Sensor Validation

### 5.2 Experiment 2: Joint Angle Recovery

**Purpose**: Validate joint angle computation from two nodes

**Setup**:
- Two nodes on virtual 2-segment arm
- Known rotation applied at joint
- Vary angle from 0° to 120°

**Metrics**:
- Joint angle error vs commanded
- Error across angle range
- Error during motion vs static

**Success Criteria**:
- Angle error < 10° across range
- No systematic bias

**Owner**: Codex / Fusion

### 5.3 Experiment 3: Biomechanical Motion

**Purpose**: Validate realistic human motion tracking

**Setup**:
- 5-6 node setup (leg + arm or both arms)
- Scripted gait or reaching motion
- Compare to motion capture ground truth

**Metrics**:
- Joint angle trajectories vs reference
- Peak error during motion
- Smoothness (jerkiness)

**Success Criteria**:
- Qualitatively matches reference
- No impossible poses (joint limits respected)

**Owner**: Codex / Fusion

### 5.4 Experiment 4: Magnetic Interference

**Purpose**: Test robustness to magnetic disturbances

**Setup**:
- Static node
- Apply simulated magnetic field disturbance
- Measure recovery

**Metrics**:
- Error during interference
- Recovery time
- Final settled error

**Success Criteria**:
- Detects interference
- Recovers to < 15° error after disturbance
- Warning issued to user

**Owner**: Codex / Sensor Validation

### 5.5 Experiment 5: Bone Length Sensitivity

**Purpose**: Quantify impact of bone length calibration errors

**Setup**:
- Two-node arm with known geometry
- Use incorrect bone lengths (+/- 10%, 20%)
- Compute joint positions

**Metrics**:
- Position error at end effector
- Error propagation analysis

**Success Criteria**:
- Document error per % of mismeasurement
- Provide calibration guidance

**Owner**: Codex / Skeleton Solver

### 5.6 Experiment 6: Multi-Node Drift

**Purpose**: Verify drift characteristics across kinematic chain

**Setup**:
- 4-node chain
- Continuous motion for 5 minutes
- Track orientation of each node

**Metrics**:
- Drift per node
- Relative drift between nodes
- Cumulative chain error

**Success Criteria**:
- Per-node drift < 10°/min
- Relative orientation preserved (chain doesn't break)

**Owner**: Codex / Integration

---

## 6. User Documentation

### 6.1 What to Tell Users

**On First Use**:
> "HelixDrift tracks your body orientation and joint angles. It does NOT track 
> your position in the room - the skeleton will move with you. For VR, your 
> headset tracks position. For animation, you can manually position the root."

**Calibration**:
> "For best results, enter your arm and leg lengths in the settings. 
> Approximate values are fine - default is based on average human proportions."

**Limitations**:
> "Keep away from metal objects and electronics - they can interfere with 
> the compass. If you see drift, stand still for 2 seconds to reset."

### 6.2 Use Case Guidance

**For VR Developers**:
- Use orientations to drive avatar joints
- Combine with HMD position for root placement
- Test in your target environment for interference

**For Animators**:
- Capture is real-time and streaming
- Root position can be keyframed separately
- Export orientation curves for refinement

**For Biomechanists**:
- Joint angles are primary output
- Segment orientations available
- Position data is relative, not global

---

## 7. Open Questions for Future Research

### 7.1 v2 Considerations

| Feature | Research Needed | Hardware |
|---------|-----------------|----------|
| Global position | UWB anchor positioning accuracy | UWB chip per node |
| Vision fusion | SLAM integration complexity | Wearable cameras or external setup |
| Floor-aware IK | Contact detection reliability | Pressure sensors (optional) |
| Multi-person | Protocol and bandwidth scaling | Same hardware, protocol changes |

### 7.2 Unresolved Technical Questions

1. **Gyro bias estimation**: Can we auto-calibrate during motion?
2. **Soft tissue motion**: How much does skin motion affect accuracy?
3. **Fast motion**: What's max angular velocity before saturation?
4. **Temperature effects**: Does sensor calibration drift with temp?
5. **Long-term drift**: Can we detect and correct over hours?

---

## 8. Summary

### 8.1 The Pitch

**"HelixDrift v1: Wearable mocap that just works"**

- 3-6 small sensors on your body
- See your skeleton move in real-time
- Joint angles accurate enough for VR, animation, sports
- 4+ hour battery life
- No cameras, no studio, no setup

**The Trade-off**:
- We track how your body is oriented, not where it is in the room
- This eliminates drift and complexity
- Position comes from your VR headset, or you animate it separately

### 8.2 The Technical Reality

**What works**:
- IMU + magnetometer fusion = stable orientation
- Forward kinematics = joint positions from orientations
- Low latency = real-time applications

**What doesn't work (without extra hardware)**:
- Double-integration for position = catastrophic drift
- Relative node positioning = each drifts independently
- Global tracking = needs UWB or cameras

### 8.3 The Recommendation

**Ship v1 with orientation-only skeleton tracking.**

It's:
- ✅ Achievable with current hardware
- ✅ Sufficient for most use cases
- ✅ Reliable and bounded error
- ✅ Implementable on timeline

Defer position tracking to v2 with UWB if market demands it.

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-03-29 | Final recommendation - orientation-only for v1 |

