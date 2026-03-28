# Pose Inference Requirements

**Status**: Draft v0.1  
**Owner**: Kimi / Pose Feasibility Research  
**Date**: 2026-03-29  

---

## 1. Executive Summary

This document defines what spatial outputs HelixDrift needs to reconstruct useful human or animal motion. The goal is to identify the **minimum viable outputs** for v1 and clarify what can be deferred or requires additional hardware.

**Key Findings (Preliminary)**:
- **v1 Minimum**: Per-segment orientation + fixed bone lengths + kinematic constraints
- **Deferred**: Relative translation between nodes (inertial drift is problematic)
- **Requires external ref**: Global position (requires UWB, vision, or floor contact)
- **Joint angles**: Derivable from orientation differences (relative rotation)

---

## 2. Output Types Defined

### 2.1 Per-Segment Orientation

| Property | Description | Source |
|----------|-------------|--------|
| Quaternion | 3D rotation of sensor relative to world | IMU + magnetometer fusion |
| Accuracy target | < 5° RMS error under motion | Fusion pipeline goal |
| Drift | < 10° over 60 seconds | Magnetometer-aided AHRS |

**Status for v1**: ✅ **REQUIRED** - This is what IMU+mag can reliably provide

### 2.2 Joint Angle

| Property | Description | Source |
|----------|-------------|--------|
| Definition | Relative rotation between two connected segments | Orientation_A⁻¹ × Orientation_B |
| Examples | Elbow flexion, knee angle, shoulder abduction | Derived from two nodes |
| Accuracy | Propagates from orientation error (±5° → ±7° joint) | Kinematic chain accumulation |

**Status for v1**: ✅ **REQUIRED** - Derivative of orientation, no extra hardware needed

### 2.3 Root Orientation

| Property | Description | Source |
|----------|-------------|--------|
| Definition | Orientation of pelvis (human) or torso (animal) | Single node designated as root |
| Purpose | Global reference for skeleton | Sets coordinate system |
| Accuracy | Same as per-segment orientation | One of the node orientations |

**Status for v1**: ✅ **REQUIRED** - Designate one node as root reference

### 2.4 Root Translation (Position)

| Property | Description | Source |
|----------|-------------|--------|
| Definition | Position of root segment in world coordinates | IMU double integration (problematic) |
| Drift | > 1 meter per minute without correction | Inertial navigation inherent drift |
| Alternatives | Floor contact detection, UWB, vision | External references required |

**Status for v1**: ⚠️ **OPTIONAL / DEGRADED** - Can estimate from gait/contact, but expect drift

### 2.5 Per-Node Relative Translation

| Property | Description | Source |
|----------|-------------|--------|
| Definition | Position of one node relative to another | IMU dead reckoning on each node |
| Drift | Accumulates quickly (~10-50 cm over 10s) | Double integration error |
| Use case | Limb extension tracking | E.g., how far arm reached |

**Status for v1**: ❌ **DEFERRED** - Too much drift without external references

### 2.6 Global Body Position

| Property | Description | Source |
|----------|-------------|--------|
| Definition | Position of entire body in world coordinates | Root position + skeleton structure |
| Drift | Same as root translation | Depends on root position accuracy |
| Requirements | External tracking system (UWB, lighthouse, cameras) | Not available from IMU alone |

**Status for v1**: ❌ **OUT OF SCOPE** - Requires additional hardware infrastructure

---

## 3. Use Case Analysis

### 3.1 VR/AR Avatar (Primary Use Case)

**What the renderer needs**:
1. ✅ Joint angles for articulation
2. ✅ Root orientation for body facing
3. ⚠️ Root height (can estimate from leg angles + floor contact)
4. ❌ Global position (VR tracks HMD separately)

**Analysis**: 
- VR systems track head (HMD) and hands (controllers) separately
- Body tracking adds limb/upper body pose
- Global position comes from HMD tracking, not body sensors
- **Conclusion**: Orientation + joint angles sufficient

### 3.2 Film/TV Animation

**What the animator needs**:
1. ✅ Joint angles for character rig
2. ✅ Root orientation for body direction
3. ⚠️ Root translation for character movement
4. ⚠️ Foot/ground contact for realistic motion

**Analysis**:
- Root translation helps, but can be hand-tweaked
- Floor contact can be inferred from leg angles
- Position drift acceptable if re-targeted to ground plane
- **Conclusion**: Orientation + joint angles + optional root position

### 3.3 Sports Biomechanics

**What the analyst needs**:
1. ✅ Joint angles (knee flexion, hip rotation)
2. ✅ Segment orientations (trunk lean, arm position)
3. ✅ Temporal dynamics (velocity, acceleration)
4. ❌ Global position rarely needed (relative motion matters)

**Analysis**:
- Focus on relative motion, body mechanics
- Ground reaction forces more important than global position
- **Conclusion**: Orientation + joint angles sufficient

### 3.4 Game Input

**What the game needs**:
1. ✅ Gestures (derived from orientation trajectories)
2. ✅ Stance/posture (current orientation state)
3. ⚠️ Movement intent (can infer from leaning)
4. ❌ Precise position (games typically use relative)

**Analysis**:
- Games need "what is player doing" not "where exactly are they"
- **Conclusion**: Orientation trajectories sufficient

---

## 4. v1 Product Definition

### 4.1 IN SCOPE (v1)

| Output | Priority | Notes |
|--------|----------|-------|
| Per-segment orientation | P0 | Core IMU+mag output |
| Joint angles (relative) | P0 | Derived from orientation |
| Root orientation | P0 | Reference frame |
| Orientation quality metrics | P0 | Confidence, error estimates |

### 4.2 OPTIONAL (v1 - Best Effort)

| Output | Priority | Notes |
|--------|----------|-------|
| Root height estimate | P1 | From leg angles + floor contact |
| Velocity estimates | P1 | Differentiated from orientation |
| Gait phase detection | P1 | Contact/st Swing detection |
| Drift warnings | P1 | Alert when accuracy degrades |

### 4.3 OUT OF SCOPE (v1)

| Output | Priority | Notes |
|--------|----------|-------|
| Global position | P2 | Requires UWB/vision |
| Relative node translation | P2 | Inertial drift too high |
| Precise foot position | P2 | Requires foot tracking |
| Body scale estimation | P2 | Requires calibration |

### 4.4 FUTURE (v2+)

| Output | Priority | Requirements |
|--------|----------|--------------|
| Global position with UWB | P2 | Additional UWB hardware |
| Vision-assisted drift correction | P2 | External camera system |
| Floor-aware IK | P2 | Environment sensing |
| Multi-person tracking | P2 | Additional nodes/protocol |

---

## 5. Technical Feasibility

### 5.1 What IMU+Magnetometer+Barometer CAN Provide

✅ **Reliable**:
- Per-segment orientation (with magnetometer aiding)
- Orientation dynamics (angular velocity)
- Gravity direction (from accelerometer)
- Magnetic north reference

⚠️ **Partial / Noisy**:
- Vertical position (barometer, noisy)
- Linear acceleration (separating gravity is hard)

❌ **Cannot Provide**:
- Absolute position (drifts immediately)
- Position relative to other nodes (each drifts independently)

### 5.2 Why Position Drifts

Fundamental physics limitation:

```
Position = ∫∫ Acceleration dt²

Problem:
1. Accelerometer noise → integration error accumulates
2. Gravity subtraction errors → bias in acceleration
3. No absolute position reference → unbounded drift

Result: > 1 meter error in < 30 seconds
```

### 5.3 Workarounds for Position

| Method | Accuracy | Complexity | v1 Suitable? |
|--------|----------|------------|--------------|
| Double integration | Poor | Low | ❌ No |
| Zero-velocity updates | Moderate | Medium | ⚠️ Maybe (foot contact) |
| Kinematic constraints | Good | High | ✅ Yes (fixed bone lengths) |
| UWB ranging | Excellent | High | ❌ No (extra hardware) |
| Visual odometry | Excellent | Very High | ❌ No (cameras required) |

---

## 6. Skeleton Reconstruction Approach

### 6.1 v1 Approach: Orientation-Driven IK

```
Inputs:
- Node orientations: q_root, q_thigh, q_shin, q_foot
- Bone lengths: L_thigh, L_shin (user-provided or default)
- Joint constraints: knee hinge, hip ball-socket

Process:
1. Place root at origin (or last known position)
2. Propagate orientations down kinematic chain
3. Compute joint positions from orientations + bone lengths
4. Apply IK to satisfy constraints (foot on ground, etc.)

Output:
- Joint positions (relative to root)
- Joint angles
- Full skeleton pose
```

### 6.2 Example: Leg Chain

```
Pelvis (root)
  └── Thigh (node 1)
        └── Shin (node 2)
              └── Foot (node 3)

Given:
- q_pelvis, q_thigh, q_shin (orientations)
- L_thigh, L_shin (bone lengths)

Compute:
- Knee_angle = angle between thigh and shin
- Hip_angle = angle between pelvis and thigh
- Ankle_angle = angle between shin and foot (if foot node)
- Joint positions via forward kinematics
```

### 6.3 Root Position Estimation (Optional)

If ground contact detected:
```
foot_on_ground = detect_contact(accel_pattern)
if foot_on_ground:
    root_height = L_thigh * cos(hip_angle) + L_shin * cos(knee_angle)
    root_position = foot_position + root_height * up_vector
```

---

## 7. Validation Requirements

### 7.1 Simulation Experiments Needed

| Experiment | Purpose | Success Criteria |
|------------|---------|------------------|
| Static pose | Baseline accuracy | < 5° orientation error |
| Simple rotation | Joint angle accuracy | Recovered angle within 3° of ground truth |
| Gait cycle | Dynamic motion | Joint angles track expected biomechanics |
| Drift test | Long-term stability | < 10° orientation drift over 60s |
| Two-node chain | Relative orientation | Elbow/knee angle correctly estimated |
| Three-node chain | Multi-segment IK | Full limb pose reasonable |

### 7.2 Metrics to Track

```
- Orientation RMS error (degrees)
- Joint angle error (degrees)
- Position drift rate (cm/minute) - if attempting position
- IK solve success rate (%)
- Pose quality score (heuristic)
```

---

## 8. Interface Implications

### 8.1 Node Output (What Each Node Sends)

```
struct NodeSample {
    uint32_t timestamp;      // Synced to master
    Quaternion orientation;  // World-relative
    uint8_t quality;         // Confidence 0-255
    uint8_t flags;           // Calibration status, etc.
};
```

### 8.2 Master Output (To Skeleton Solver)

```
struct SkeletonInput {
    uint8_t node_count;
    NodeSample nodes[MAX_NODES];
    BoneLength bone_lengths[MAX_BONES];  // User config
    SkeletonTopology topology;           // Joint connectivity
};
```

### 8.3 Downstream Output (To Renderer/Game)

```
struct SkeletonPose {
    Joint joints[MAX_JOINTS];  // Position + orientation
    float quality_score;
    uint8_t confidence_per_joint[MAX_JOINTS];
};
```

---

## 9. Recommendations

### 9.1 v1 Product Scope

**MUST HAVE**:
1. Per-node orientation streaming
2. Joint angle computation (relative rotation)
3. Basic skeleton solver (forward kinematics)
4. Orientation quality metrics

**SHOULD HAVE**:
5. Simple IK for ground contact
6. Bone length calibration utility
7. Drift monitoring/warnings

**WON'T HAVE (v1)**:
8. Global position tracking
9. UWB integration
10. Vision fusion

### 9.2 Success Criteria for v1

- User can wear 3-6 nodes and see articulated skeleton
- Joint angles are qualitatively correct (bends when limb bends)
- Orientation drift < 10° over 1 minute of motion
- Latency < 50 ms from motion to display

### 9.3 Handoff to Implementation

**For Fusion Team (Codex)**:
- Validate orientation accuracy claims
- Implement quaternion interpolation/filtering
- Provide quality/confidence metrics

**For Skeleton Solver (future team)**:
- Implement forward kinematics from orientations
- Add joint constraint enforcement
- Handle bone length configuration

---

## 10. Open Questions

1. **Bone length calibration**: User-measured or auto-estimated?
2. **Joint constraints**: Pre-defined skeleton or user-configurable?
3. **Root tracking**: Is floor contact sufficient for height?
4. **Multi-node drift**: Do multiple nodes drift coherently or independently?
5. **Animal skeletons**: Same approach works for quadrupeds?

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-03-29 | Initial requirements - v1 scope defined |

