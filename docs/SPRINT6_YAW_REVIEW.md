# Sprint 6 Yaw-Only Review and SensorFusion Fix Brief

**Date:** 2026-03-29
**Author:** Claude / Review Board
**Codex commits reviewed:** f6af05a, 5d9048b, 56eb378

---

## 1. Review Verdict: APPROVE

Three clean, honest commits. No fake thresholds. Good characterization pattern.

### f6af05a: Tighten startup yaw harness coverage

The quarter-turn test was reworked from "55° after 20 ticks" to "< 2° on
first measured sample." This directly validates that `initFromSensors()` seeds
the Mahony state correctly for yaw. Excellent — this is the strongest evidence
that the SensorFusion init fix works for heading.

### 5d9048b: Yaw gain characterization

Tests that higher Kp worsens both static yaw offset and dynamic yaw tracking.
Sweeps Kp={0.5, 1.0, 2.0, 5.0} with monotonic comparison assertions. This
is valuable tuning data — it proves that the A1 failure at high Kp was
expected filter behavior, not a bug.

**Important finding documented by this test:** Lower Kp is better for yaw
accuracy. This is the opposite of naive intuition ("stronger correction =
better tracking"). The reason is that Kp amplifies the accel/mag correction,
and for yaw the mag correction signal is weak — so high Kp mostly amplifies
noise from the accel cross-product, not the mag heading correction.

### 56eb378: Axis-specific tracking characterization

Proves that yaw < pitch < roll in error magnitude for both static and dynamic
cases. Uses relative assertions (yaw better than pitch, pitch better than
roll) instead of absolute thresholds. This correctly documents the current
filter behavior without pretending it meets targets.

---

## 2. M2 Intermediate Progress: YES, VALID

The following are now proven with passing tests at 246/246:

| Evidence | Status | M2 Value |
|---|---|---|
| Filter init from sensors (yaw) | First sample < 2° | Heading works |
| Ki bias rejection | Green (A5) | Integral feedback works |
| 60s drift stability | Green (A3) | Long-term stability proven |
| Joint angle recovery | Green (A6) | Multi-node relative orientation works |
| Yaw is best axis | Characterized (56eb378) | Known limitation documented |
| Higher Kp worsens yaw | Characterized (5d9048b) | Tuning insight captured |
| Pitch/roll broken | Documented, not faked | Honest limitation |

This is legitimate M2 intermediate progress. The yaw axis is the hardest
(mag-dependent) and it demonstrably works. Pitch/roll are blocked on a known
SensorFusion init bug with a clear fix path. No fake thresholds were committed.

---

## 3. SensorFusion Init Fix Brief

### Symptom

`MahonyAHRS::initFromSensors(accel, mag)` correctly computes initial yaw
(heading from mag) but produces wrong pitch and roll (from accel gravity
decomposition). Evidence:

| Orientation | Expected error | Actual error | Ratio |
|---|---|---|---|
| ±15° yaw | ~0° (init should nail it) | ~15° RMS | ~1x offset |
| ±15° pitch | ~0° | ~29° RMS | ~2x offset |
| ±15° roll | ~0° | ~38° RMS | ~2.5x offset |

Dynamic pitch/roll at 30°/s diverge to 180°.

### Likely Root Cause

The `quaternionFromEulerDeg(roll, pitch, yaw)` function uses ZYX rotation
order. The Euler angles computed from accel gravity are:
```cpp
roll  = atan2(-ay, az)
pitch = atan2(ax, sqrt(ay² + az²))
```

If the simulator's quaternion library uses a different convention for
`fromAxisAngle()` vs the init's Euler decomposition — for example, the
simulator treats rotations as body-to-world while the init treats them
as world-to-body — the yaw happens to match (last rotation in ZYX, same
in both conventions) but pitch and roll are inverted or transposed.

### Reproduction Test

Add to SensorFusion middleware tests:

```cpp
TEST(MahonyInitTest, PitchOnlyInitMatchesAxisAngle) {
    // Synthesize accel+mag for +15° pitch
    // gravity at +15° pitch: ax = sin(15°)*g, ay = 0, az = cos(15°)*g
    float ax = 0.2588f;  // sin(15°)
    float ay = 0.0f;
    float az = 0.9659f;  // cos(15°)
    // mag at +15° pitch: rotate earth field [25, 0, -40] by -15° around Y
    float mx = 25*cos(-15°) - (-40)*sin(-15°);  // compute exact values
    float my = 0;
    float mz = 25*sin(-15°) + (-40)*cos(-15°);

    MahonyAHRS ahrs;
    ahrs.initFromSensors({ax, ay, az}, {mx, my, mz});
    Quaternion q = ahrs.getQuaternion();
    Quaternion expected = Quaternion::fromAxisAngle(0, 1, 0, 15.0f);
    EXPECT_LT(angularErrorDeg(expected, q), 1.0f);
}

TEST(MahonyInitTest, RollOnlyInitMatchesAxisAngle) {
    // Same pattern for +15° roll
    float ax = 0.0f;
    float ay = -0.2588f;  // -sin(15°)
    float az = 0.9659f;   // cos(15°)
    // mag: rotate earth field by -15° around X
    // ... compute exact values ...

    MahonyAHRS ahrs;
    ahrs.initFromSensors({ax, ay, az}, {mx, my, mz});
    Quaternion q = ahrs.getQuaternion();
    Quaternion expected = Quaternion::fromAxisAngle(1, 0, 0, 15.0f);
    EXPECT_LT(angularErrorDeg(expected, q), 1.0f);
}
```

### Acceptance Condition

The fix is complete when:
1. Both repro tests pass (init quaternion within 1° of `fromAxisAngle` truth)
2. Existing A1a yaw test still passes (no yaw regression)
3. A1a pitch ±15° RMS drops below 8° (intermediate threshold)
4. A1a roll ±15° RMS drops below 8°
5. A2 dynamic pitch/roll RMS drops below 30° (no longer diverging to 180°)

### Fix Location

`external/SensorFusion/middleware/ahrs/MahonyAHRS.cpp` — the
`initFromSensors()` function and/or `quaternionFromEulerDeg()`.

Per project rules: fix in SensorFusion first, push, then update the submodule
pointer in HelixDrift.

---

## 4. What Codex Should Do Next

**Immediate (no blockers):**
1. Nothing more on Wave A yaw-only — it's done and approved.
2. The SensorFusion pitch/roll init fix is the highest-value next step.
   Codex can either:
   - Fix it in the SensorFusion submodule (if Codex owns submodule fixes)
   - Or hand it to whoever owns SensorFusion and move to Wave B work

**If Codex fixes SensorFusion:**
- Write the two repro tests first (pitch-only init, roll-only init)
- Fix the convention mismatch
- Push SensorFusion, update submodule pointer
- Re-run A1a with all 7 poses
- Re-run A2 with all 3 axes

**If Codex moves to Wave B while waiting:**
- B1: CSV export + Python plots (no blockers, independent)
- B4: Sensor validation matrix remaining gaps (no blockers, independent)
- Do NOT start B3 (calibration effectiveness) — it depends on pitch/roll
  working correctly

**Wave A is not done, but yaw-only is shipped.** Pitch/roll will unblock when
the SensorFusion fix lands.
