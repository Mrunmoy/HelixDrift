# Sprint 5: Mainline Redirect After A1 Escalation

**Date:** 2026-03-29
**Author:** Claude / Systems Architect
**Trigger:** A1 static ±90° yaw shows 118° RMS — catastrophic non-convergence

---

## 1. Current Mainline Assessment

**What works:**
- A5 (Ki bias rejection): passing, validated, approved
- Harness infrastructure: solid, well-tested
- Identity pose: near-zero error (filter starts aligned)
- Small perturbations around identity: track correctly

**What is broken:**
- Static ±90° yaw: 118° RMS, 129° max after 2s warmup
- Raising Kp makes it worse, not better
- This is NOT a threshold problem — the filter is diverging from the target

**Root cause hypothesis:**

The Mahony filter starts at identity. When the gimbal snaps to 90° yaw and
sensors report data for that orientation, the filter should converge toward
90° yaw. Instead it diverges to ~118-129° error. This means the Mahony
correction is pushing the wrong direction for large yaw offsets.

Most likely cause: the Mahony 9-DOF update computes the mag reference
direction relative to the *current estimate*, not the truth. When the
estimate is far from truth (90° off), the reference computation produces a
correction that overshoots or spirals. This is a known limitation of
complementary filters — they linearize around the current estimate, and the
linearization breaks for large errors.

**Why increasing Kp makes it worse:** Higher Kp amplifies the (incorrect)
correction vector. If the correction direction is wrong, stronger correction
= faster divergence.

**Why identity works:** When the filter estimate equals truth, the
linearization is exact and the correction is zero. Small perturbations stay
in the linear regime.

**Why A5 works:** A5 starts at the correct orientation and applies a small
gyro bias. The filter drifts slowly (stays in the linear regime) and Ki
pulls it back. The filter never gets 90° away from truth.

---

## 2. What This Means for Wave A

A1 as originally specified (arbitrary static poses, filter starts at identity)
is **blocked by Mahony filter behavior**, not by Codex implementation quality.
The filter cannot converge from large initial errors via sensor corrections
alone.

This is not a bug — it is a fundamental characteristic of complementary
filters. Real hardware avoids this because the filter initializes from the
first accel+mag reading (not from identity), so it starts close to truth.

---

## 3. Revised Wave A Order

### Continue now

| Task | Why | Notes |
|------|-----|-------|
| **A3: 60s drift** | Tests stability near truth (filter stays aligned). Uses Ki. Directly valuable. | Use Ki=0.02, identity start. Should pass. |
| **A2: Dynamic tracking** | Tests tracking from identity with rotation. Filter stays near truth because it tracks incrementally. | Start with yaw, then pitch/roll. |
| **A6: Two-node joint angle** | Both nodes start at their target orientations (identity and flexion). Filter converges from nearby. | Works if we fix the initialization. See below. |

### Rescope A1

**A1 should be split into two tasks:**

**A1a: Static accuracy at identity and small offsets (EXECUTABLE NOW)**
- Test identity, ±15° yaw, ±15° pitch, ±15° roll (7 poses)
- These are within the Mahony linear convergence regime
- Use intermediate thresholds: RMS < 8°, max < 15°
- This validates that the filter + simulator produce correct orientation
  when the initial error is small

**A1b: Static accuracy at large offsets — BLOCKED on filter initialization**
- ±90° yaw, ±45° pitch are blocked until the pipeline supports proper
  first-sample initialization
- The fix: `MahonyAHRS::reset()` should initialize from the first
  accel+mag reading instead of identity. Or add a `setQuaternion(q)`
  method that tests can use to pre-align the filter.
- This is a **SensorFusion change**, not a simulator or test change.

### Rescope A4

**A4 (convergence sweep) should test from SMALL initial errors only:**
- Snap from identity to 15° yaw (not 90°)
- Measure convergence time for various Kp/Ki
- 90° convergence is blocked by the same filter initialization issue

### Defer

| Task | Why |
|------|-----|
| A1b (large-offset static accuracy) | Blocked on SensorFusion filter init |
| A4 large-angle convergence | Same blocker |

---

## 4. Escalation: SensorFusion Filter Initialization

**Problem:** MahonyAHRS always starts at identity quaternion. There is no way
to initialize it from sensor data. This means any test that starts the filter
far from truth will fail to converge.

**Evidence:**
- ±90° yaw: 118° RMS after 2s (diverges)
- Higher Kp makes it worse (amplifies wrong correction)
- Identity and small offsets work fine

**Proposed fix (two options):**

Option A: Add `MahonyAHRS::initFromSensors(AccelData, MagData)`
- Computes initial quaternion from first accel+mag reading using TRIAD or
  similar one-shot alignment
- Called once before the filter loop starts
- This is what real implementations do

Option B: Add `MahonyAHRS::setQuaternion(Quaternion q)`
- Allows tests to pre-align the filter to the gimbal truth
- Simpler but less realistic — tests would bypass the initialization problem
  rather than solving it

**Recommendation:** Option A for production, Option B as a quick unblock for
testing. Both are small changes (~10-20 lines in MahonyAHRS).

**Owner:** This change belongs to the SensorFusion submodule. Per project
rules: fix in SensorFusion first, push, then update the submodule pointer in
HelixDrift.

---

## 5. What Counts as M2 Progress Now

Even without A1b/A4 large-angle cases, M2 can make meaningful progress:

| Evidence | Status | M2 Value |
|---|---|---|
| A5 Ki bias rejection | Done | Proves integral feedback works |
| A3 60s drift at identity | Next | Proves long-term stability |
| A2 dynamic tracking from identity | Next | Proves rotation tracking |
| A1a small-offset accuracy | Next | Proves static accuracy in linear regime |
| A6 two-node joint angle | After A1a | Proves multi-node relative orientation |
| Filter init fix (SensorFusion) | Escalated | Unblocks A1b and A4-large |

M2 is NOT blocked. It is narrowed. The achievable M2 scope is:
"orientation tracking is accurate when the filter starts near truth, drift
is bounded by Ki, and joint angles are recoverable." This is a legitimate
and useful result — it proves the pipeline works, with the known caveat
that filter initialization needs improvement.

---

## 6. Decision Note for Codex

### Do next (in order)

1. **A3: 60s drift test.** Config: Kp=1.0, Ki=0.02, identity start, clean
   sensors, setSeed(42). 3000 ticks. Assert: max error < 10°, drift rate
   < 5°/min. This should pass.

2. **A1a: Small-offset static accuracy.** 7 poses: identity, ±15° yaw,
   ±15° pitch, ±15° roll. 100-tick warmup, 200-tick measurement. Assert:
   per-pose RMS < 8°, max < 15°. This should pass.

3. **A2: Dynamic single-axis tracking.** Yaw first, then pitch/roll. 30°/s
   from identity. 50-tick warmup, 500-tick motion. Assert: RMS < 15°,
   max < 30°.

4. **A6: Two-node joint angle.** Two harnesses. Both init at identity.
   Set child gimbal to flexion angles {30, 60, 90}°. Let both converge
   from identity (not from the target). Measure joint angle after 250
   ticks. Assert: error < 10°.

### Do NOT do

- Do not attempt ±90° yaw or ±45° pitch static poses.
- Do not attempt convergence from large initial errors.
- Do not tighten A5 thresholds beyond current values (they are fine).
- Do not add noise injection until the above pass clean.

### Escalate

- File a SensorFusion issue: "MahonyAHRS needs first-sample initialization
  from accel+mag (TRIAD or equivalent). Filter cannot converge from large
  initial errors."
- Optionally add `setQuaternion(q)` as a quick test-only unblock.
- Once the SensorFusion fix lands and the submodule is updated, A1b and
  A4-large become unblocked.
