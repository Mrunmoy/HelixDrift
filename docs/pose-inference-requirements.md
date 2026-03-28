# Pose Inference Requirements

**Status**: v1.0
**Team**: Pose Inference
**Date**: 2026-03-29

---

## 1. Purpose

This document defines the spatial outputs that the downstream skeleton system needs from each HelixDrift sensor node. It establishes which outputs are required for v1, which are deferred, and what accuracy is needed for useful motion capture.

**Core constraint**: HelixDrift nodes carry IMU (LSM6DSO), magnetometer (BMM350), and barometer (LPS22DF). These sensors can estimate **orientation** reliably. They **cannot** estimate **position** without unbounded drift. RF transport (BLE) does not provide spatial localization. v1 must be orientation-first.

---

## 2. Output Definitions

### 2.1 Segment Orientation (quaternion per node)

The absolute orientation of a body segment in world frame, expressed as a unit quaternion (w, x, y, z).

| Property | Value |
|----------|-------|
| **Priority** | **REQUIRED for v1** |
| **Sensor dependency** | LSM6DSO (accel + gyro) + BMM350 (mag) via Mahony AHRS |
| **Accuracy target** | < 5 deg RMS static, < 10 deg RMS dynamic |
| **Update rate** | 50 Hz (performance), 40 Hz (battery) |
| **Failure mode** | Wrong segment orientation propagates directly to wrong limb pose. Most visible error type -- even 10 deg on a forearm segment produces ~5 cm endpoint error on a 50 cm limb. |

**Notes**: This is the primary output of each node. The current pipeline already produces this via `MocapNodeSample::orientation`. 9-DOF fusion (accel + gyro + mag) provides heading-stabilized orientation; 6-DOF fallback (accel + gyro only) loses heading but maintains pitch/roll.

---

### 2.2 Joint Angles (relative orientation between adjacent nodes)

The relative rotation between two adjacent body segments, computed as `q_child * conjugate(q_parent)`. Represents the anatomical joint angle (e.g., elbow flexion/extension).

| Property | Value |
|----------|-------|
| **Priority** | **REQUIRED for v1** |
| **Sensor dependency** | Two segment orientations (from nodes on adjacent segments) + known skeleton topology |
| **Accuracy target** | < 5 deg RMS for major joints (elbow, knee), < 8 deg RMS for complex joints (shoulder, hip) |
| **Update rate** | Same as segment orientation (50 Hz) |
| **Failure mode** | Incorrect joint angle causes unnatural poses (hyperextension, impossible rotations). Can be partially mitigated by joint limit constraints. |

**Notes**: Computed on the master/receiver side, not on-node. Requires synchronized timestamps between adjacent nodes (< 1 ms inter-node skew, per RF/Sync requirements). Error compounds from both nodes' orientation errors.

---

### 2.3 Root Orientation (pelvis/torso reference frame)

The orientation of the root segment (typically pelvis or lower torso) in world frame. Defines the character's facing direction and tilt.

| Property | Value |
|----------|-------|
| **Priority** | **REQUIRED for v1** |
| **Sensor dependency** | Single node (pelvis-mounted): LSM6DSO + BMM350 via Mahony AHRS |
| **Accuracy target** | < 3 deg RMS heading, < 2 deg RMS pitch/roll |
| **Update rate** | 50 Hz |
| **Failure mode** | Heading drift causes entire character to slowly rotate in world space. Pitch/roll error causes character to lean incorrectly. Heading is the most vulnerable axis -- magnetic interference or poor calibration directly impacts this. |

**Notes**: Stricter accuracy requirements than limb segments because root orientation errors propagate to every child segment in the kinematic chain. Magnetic heading quality is critical here. Hard/soft iron calibration on the pelvis node must be high quality.

---

### 2.4 Root Translation (movement of root in world space)

The position of the root segment (pelvis) in world coordinates over time. Tracks how the character moves through space.

| Property | Value |
|----------|-------|
| **Priority** | **DEFERRED (not v1)** |
| **Sensor dependency** | Would require: double-integrated accelerometer (drifts unboundedly), or external reference (UWB, vision, floor contact) |
| **Accuracy target** | < 10 cm RMS for useful mocap (unreachable with IMU alone) |
| **Update rate** | N/A for v1 |
| **Failure mode** | Without root translation, character is anchored to a fixed point. Acceptable for many applications (seated VR, upper-body tracking, animation retargeting). Unacceptable for full-body locomotion capture. |

**Notes**: IMU dead reckoning drifts at approximately 1-10 m after 10 seconds of walking due to double integration of noisy accelerometer data. This is a fundamental physics limitation, not a calibration problem. Requires external aiding (see feasibility analysis). For v1, the skeleton solver can use a fixed root or simple heuristics (foot contact, gait model).

---

### 2.5 Per-Node Relative Translation

The translational displacement of individual segments relative to their parent joint. Would capture effects like shoulder shrug, spine compression, or limb lengthening.

| Property | Value |
|----------|-------|
| **Priority** | **DEFERRED (not v1)** |
| **Sensor dependency** | Double-integrated accelerometer with gravity removal (drifts rapidly) |
| **Accuracy target** | < 2 cm for useful contribution (unreachable with IMU alone) |
| **Update rate** | N/A for v1 |
| **Failure mode** | Without this, skeleton uses fixed bone lengths. Loses subtle motions (shrugs, bouncing). Acceptable for most applications. |

**Notes**: The barometer (LPS22DF) could theoretically contribute vertical displacement, but at ~10 cm altitude resolution (limited by pressure noise of ~0.01 hPa = ~8 cm altitude), it is too coarse for segment-level translation. More useful for floor-level detection (see feasibility analysis). For v1, fixed bone lengths from a calibration T-pose are sufficient.

---

### 2.6 Global Body Position

The absolute position of the character in a mapped environment (room-scale coordinates).

| Property | Value |
|----------|-------|
| **Priority** | **DEFERRED (not v1)** |
| **Sensor dependency** | Requires external infrastructure: UWB anchors, camera tracking, SLAM, or similar |
| **Accuracy target** | < 30 cm for room-scale, < 5 cm for stage-quality mocap |
| **Update rate** | N/A for v1 |
| **Failure mode** | Without global position, character exists in a local coordinate frame only. Cannot interact with environment geometry. |

**Notes**: Completely outside the scope of on-body IMU+mag+baro sensing. Requires infrastructure that HelixDrift does not currently include. Can be provided by an external system and fused with HelixDrift orientation data at the application layer.

---

## 3. Requirements Summary

| Output | Priority | Sensor Source | Accuracy Target | Computed Where |
|--------|----------|---------------|-----------------|----------------|
| Segment orientation | **REQUIRED v1** | IMU + Mag (per node) | < 5 deg RMS static, < 10 deg dynamic | On-node (Mahony AHRS) |
| Joint angles | **REQUIRED v1** | Two adjacent segment orientations | < 5 deg RMS major joints | Master (quaternion math) |
| Root orientation | **REQUIRED v1** | IMU + Mag (pelvis node) | < 3 deg heading, < 2 deg pitch/roll | On-node (Mahony AHRS) |
| Root translation | **DEFERRED** | External reference needed | < 10 cm RMS | N/A for v1 |
| Per-node translation | **DEFERRED** | Double-integrated accel (drifts) | < 2 cm | N/A for v1 |
| Global body position | **DEFERRED** | External infrastructure | < 30 cm room-scale | N/A for v1 |

---

## 4. v1 Architecture: Orientation-Only Segment Tracking

### 4.1 Approach

Each HelixDrift node streams its fused orientation quaternion at 50 Hz. The master receiver collects synchronized quaternions from all nodes and fits them to a kinematic skeleton model with known bone lengths (measured during a calibration T-pose).

```
Node 1 (pelvis)    ─── q1 ───┐
Node 2 (L thigh)   ─── q2 ───┤
Node 3 (L shin)    ─── q3 ───├──► Master: Skeleton Solver ──► Pose Output
Node 4 (R thigh)   ─── q4 ───┤      (fixed bone lengths,
Node 5 (R shin)    ─── q5 ───┤       joint constraints)
Node 6 (torso)     ─── q6 ───┘
```

### 4.2 What This Gets You

- Full upper-body and lower-body joint angles
- Character facing direction (magnetic heading)
- Natural-looking poses via joint limit constraints
- Sufficient for: VR avatars (seated/standing), animation retargeting, sports analysis, rehabilitation

### 4.3 What This Does Not Get You

- Locomotion (walking/running translation) -- character stays in place
- Absolute room position -- no spatial anchoring
- Subtle translational motions (shrugs, bouncing, spine flex)

### 4.4 Mitigation Strategies for Deferred Outputs

| Missing Output | v1 Workaround |
|----------------|---------------|
| Root translation | Fixed root position, or simple gait model (detect foot contact via accelerometer, estimate stride length) |
| Per-node translation | Fixed bone lengths from T-pose calibration |
| Global position | Application layer provides external tracking if needed |

---

## 5. Accuracy Budget

Error compounds through the kinematic chain. For a 6-node half-body chain:

| Source | Per-Node Error | Chain Propagation |
|--------|---------------|-------------------|
| Sensor noise (accel) | ~0.5 deg | Filtered by Mahony AHRS |
| Gyro drift (with Ki) | ~0.5 deg/min steady-state | Accumulates over chain length |
| Magnetic heading error | 1-3 deg (calibrated) | Affects all nodes' yaw |
| Inter-node sync skew | < 1 ms = < 0.5 deg at 500 deg/s | Worst case during fast motion |
| **Total per-node** | **~2-5 deg RMS** | -- |
| **Endpoint error** (50 cm limb) | -- | **~2-4 cm per segment** |
| **Full chain** (pelvis to hand, ~1.5 m) | -- | **~5-15 cm endpoint** |

These numbers are achievable with well-calibrated sensors and tuned Mahony filter (Kp ~1.0, Ki ~0.05). The simulation stack should validate these targets before hardware.

---

## 6. Dependencies and Interfaces

### 6.1 What Pose Inference Needs from Other Teams

| Dependency | Team | Interface |
|------------|------|-----------|
| Quaternion stream per node | Firmware/Fusion | `MocapNodeSample::orientation` at 50 Hz |
| Synchronized timestamps | RF/Sync | < 1 ms inter-node skew via `TimestampSynchronizedTransport` |
| Calibration data | Calibration | Hard/soft iron correction, gyro bias, sensor alignment |
| Skeleton definition | Application | Bone lengths, joint topology, joint limits |

### 6.2 What Pose Inference Provides to Downstream

| Output | Consumer | Format |
|--------|----------|--------|
| Per-segment orientation | Skeleton solver | Quaternion (w, x, y, z) per node, timestamped |
| Joint angles | Animation/Retargeting | Relative quaternion per joint |
| Root orientation | Character controller | World-frame quaternion for pelvis |

---

## 7. Simulation Validation Requirements

Before hardware, the simulator stack must validate:

| Test | Target | Method |
|------|--------|--------|
| Static orientation accuracy | < 5 deg RMS | VirtualGimbal at known orientations, compare fused vs truth |
| Dynamic tracking accuracy | < 10 deg RMS | Motion scripts (slow/fast rotation), measure angular error |
| Heading stability | < 3 deg drift over 60s | Stationary test with calibrated mag |
| Multi-node sync impact | < 0.5 deg from sync error | Simulate timestamp jitter on joint angle computation |
| Joint angle accuracy | < 5 deg RMS | Two-node gimbal test with known relative rotation |
| Calibration effectiveness | 2x error reduction | Compare pre/post calibration orientation error |

These tests align with the existing `SimulationHarness` design and `CALIBRATION_SIM_BRAINSTORM.md` test plan.

---

## 8. Recommendation

**v1 uses orientation-only segment tracking with kinematic skeleton fitting.**

- Each node outputs a fused orientation quaternion (already implemented in the pipeline).
- The master receiver computes joint angles from adjacent node orientations.
- A skeleton solver with fixed bone lengths and joint constraints produces the final pose.
- Root translation and global position are deferred until external aiding (UWB, vision) is available.

This approach maximizes what IMU+mag sensors can deliver reliably, avoids the fundamental drift problem of position estimation, and provides a useful mocap system for the majority of target applications.
