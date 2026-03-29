# Wave A Acceptance Guide

**Sprint:** Claude Org Sprint 4
**Date:** 2026-03-29
**Purpose:** Realistic task-by-task guidance for Codex Wave A, with intermediate
thresholds, predicted failure points, and escalation criteria.

---

## Fusion Stack Reality Check

The Mahony AHRS implementation has these characteristics that affect
what thresholds are achievable:

1. **Filter starts at identity.** If gimbal starts at identity, error is ~0
   from frame 1. If gimbal starts elsewhere, convergence takes time.
2. **Default Kp=1.0, Ki=0.0.** No integral feedback means gyro bias causes
   unbounded drift. Ki must be > 0 for any long-duration test.
3. **dt is fixed at 0.02s** (from pipeline config). Gimbal steps must match.
4. **9-DOF update** uses accel + mag. If mag data is noisy or the earth field
   model disagrees with the filter's expectation, heading can be slow to
   converge.
5. **The filter does NOT handle initial misalignment well.** Convergence from
   90° with Kp=1.0 takes ~2-3 seconds. From 180°, it may not converge at all
   (a known Mahony limitation — the error gradient flattens near 180°).

**Existing evidence from Codex tests:**
- Flat pose (identity start): < 15° after 5 ticks — should be < 5° with
  more ticks
- Constant yaw 0.314 rad/s for 0.5s: < 35° — this is tracking lag, not
  divergence
- Quarter-turn (45° snap): < 55° after 0.4s — insufficient convergence time

---

## Task-by-Task Guidance

### A1: Static Multi-Pose Orientation Accuracy

**Status:** Executable now.

**Likely outcome:** Most poses will pass at < 3° RMS with clean sensors after
5-second convergence. The filter starts at identity and then receives correct
sensor data for the target orientation — convergence should be fast.

**Predicted failure points:**
- **±90° pitch (sensor near gimbal lock):** Mahony pitch/roll correction uses
  cross-product of accel × estimated gravity. Near ±90° pitch, the cross-product
  signal becomes weak on one axis. Expect slightly higher error (3-5° instead
  of 1-2°).
- **Any orientation where mag alignment is poor:** The default earth field is
  [25, 0, -40] uT. At orientations where the horizontal field component
  projects weakly onto the sensor frame, heading correction is weaker.

**Intermediate thresholds (use these first, tighten later):**

| Threshold | Target | Intermediate | Stop tightening when |
|-----------|--------|-------------|---------------------|
| Per-pose RMS | < 3° | < 8° | < 8° with Kp=1.0, Ki=0 is the floor |
| Overall RMS | < 5° | < 10° | < 10° is the floor without Ki |
| Max single frame | < 10° | < 20° | Convergence transient is the cause |

**Recommended approach:**
1. Start with identity and ±90° yaw only (3 poses). These are easiest.
2. Add ±45° pitch. Expect slightly worse.
3. Add ±30° roll. Similar to pitch.
4. If any pose exceeds 8° RMS after 200 steady-state samples, log the actual
   values and move on — do not fake the threshold.

**Escalation:** If identity-start RMS exceeds 5° after 5 seconds with clean
sensors and Kp=1.0, there is a filter or simulator bug. Escalate to
architecture review.

---

### A2: Dynamic Single-Axis Tracking

**Status:** Executable now. Codex has already done scripted yaw regression tests.

**Likely outcome:** Yaw tracking should be reasonable (< 10° RMS at 30°/s).
Pitch and roll tracking may differ because the accel correction has different
authority on different axes.

**Predicted failure points:**
- **Yaw tracking with Kp=1.0:** The mag correction fights the gyro during
  rotation. If Kp is too high, the accel/mag reference pulls against the gyro
  and creates oscillation. At 30°/s this should be fine.
- **Roll tracking:** Least constrained by mag (mag primarily corrects heading).
  Roll correction comes only from accel cross-product. Should track well.

**Intermediate thresholds:**

| Threshold | Target | Intermediate | Stop tightening when |
|-----------|--------|-------------|---------------------|
| Per-axis RMS | < 8° | < 15° | < 15° is the floor for 30°/s with Kp=1.0 |
| Max error | < 20° | < 30° | Transient at start/stop of rotation |

**Recommended approach:**
1. Start with yaw only — extends existing scripted yaw work.
2. Add pitch, then roll.
3. If RMS > 15° at 30°/s, try with Kp=2.0 to see if it helps. If it does,
   the default Kp=1.0 is too conservative for dynamic tracking and that's
   a tuning finding, not a bug.

**Escalation:** If tracking diverges (error > 45°) or NaN appears, escalate
immediately — this indicates a filter or simulator bug.

---

### A3: Long-Duration Heading Drift (60s)

**Status:** Needs Ki > 0. With Ki=0 (default), gyro bias causes unbounded
drift. Even in simulation with zero injected bias, there may be a residual
numerical drift.

**Likely outcome with Ki=0.02:** Drift should be bounded. The integral term
will slowly cancel any residual gyro offset. Clean sensors + Ki > 0 should
achieve < 5°/min.

**Predicted failure points:**
- **Ki=0.0:** Test WILL fail. Drift may reach 10-30° over 60 seconds depending
  on residual gyro bias from the simulator. This is correct behavior — it
  validates that Ki is necessary.
- **Ki too high:** Ki > 0.1 may cause integral windup and oscillation over 60s.
- **dt accumulated error:** 3000 steps of quaternion integration may accumulate
  float precision drift. Unlikely to be significant but worth noting.

**Intermediate thresholds:**

| Threshold | Target | Intermediate | Stop tightening when |
|-----------|--------|-------------|---------------------|
| Max error 60s | < 10° | < 20° | If > 20° with Ki=0.02, escalate |
| Drift rate | < 2°/min | < 5°/min | If > 5° with Ki=0.02, escalate |

**Recommended approach:**
1. First run with Ki=0.0 and verify drift IS present (validation of the test).
   Record the drift rate. Do not assert it must be small — assert it IS positive.
2. Then run with Ki=0.02 and measure improvement.
3. Only tighten to < 2°/min if Ki=0.02 actually achieves it.

**Escalation:** If Ki=0.02 produces drift > 5°/min with clean sensors, the
Mahony integral feedback may not be working correctly. Check that `integralX_`
etc. are accumulating. This would be a SensorFusion bug.

---

### A4: Mahony Kp/Ki Convergence Sweep

**Status:** Needs softened thresholds. The original spec demands T5 < 2s for
Kp≥1.0 from a 90° snap. This may be achievable but depends on which axis
the 90° error is on.

**Likely outcome:** Convergence time depends heavily on the error axis:
- **Yaw error (90° around Z):** Corrected by mag. With Kp=1.0, convergence
  from 90° yaw takes ~2-4 seconds (mag correction is relatively weak).
- **Pitch/roll error:** Corrected by accel. With Kp=1.0, convergence from
  90° pitch/roll takes ~1-2 seconds (gravity is a strong reference).
- **180° error:** Mahony may not converge at all — the error gradient is zero
  at the antipodal point. Do NOT test 180° initial error.

**Predicted failure points:**
- **Kp=0.5 from 90°:** May NOT converge within 5s on yaw axis. This is not
  a bug — Kp=0.5 is deliberately weak.
- **High Kp with noise:** Kp=5.0 amplifies sensor noise. Without noise injection
  this is fine, but note it for later.

**Intermediate thresholds:**

| Config | Target T5 | Intermediate T5 | Stop tightening when |
|--------|-----------|-----------------|---------------------|
| Kp=0.5 | < 5s | < 8s (or document non-convergence) | Non-convergence is a valid finding |
| Kp=1.0 | < 2s | < 4s | < 4s is the floor for yaw axis |
| Kp=2.0 | < 1s | < 2s | Expected |
| Kp=5.0 | < 0.5s | < 1s | Expected |

**Recommended approach:**
1. Test yaw-axis 90° snap only first (one axis, multiple Kp values).
2. If yaw convergence at Kp=1.0 exceeds 4s, this is a mag-correction-strength
   finding, not a bug.
3. Test pitch 90° snap separately — should converge faster.
4. Do NOT test 180° error. Mahony is known to not handle this well.
5. Ki variations: compare Ki=0.0 vs Ki=0.05 steady-state error after
   convergence. Ki should reduce residual error.

**Escalation:** If Kp=5.0 from 90° does not converge to < 5° within 2s,
there is a bug in the filter or the sensor data path.

---

### A5: Gyro Bias Rejection via Ki

**Status:** Executable now. This is the single most valuable Wave A test.

**Likely outcome:** Clean separation between Ki=0 (linear drift) and Ki>0
(bounded drift). The test validates the fundamental Mahony integral mechanism.

**Predicted failure points:**
- **Bias too large:** 0.01 rad/s = 0.57°/s. With Ki=0, drift should be ~0.57°/s
  = 34°/min. If Ki=0.05 cannot bound a 34°/min drift within 30s, the Ki term
  is too weak for this bias. Try 0.005 rad/s bias instead.
- **Ki oscillation:** Ki=0.1 with 0.01 rad/s bias may cause the integral to
  overshoot and oscillate. This is not a bug — it reveals the Ki stability
  boundary.

**Intermediate thresholds:**

| Config | Target | Intermediate | Stop tightening when |
|--------|--------|-------------|---------------------|
| Ki=0 drift rate | > 5°/min | > 20°/min (at 0.01 rad/s bias) | Must show drift |
| Ki=0.05 drift rate | < 2°/min | < 5°/min | If > 10°, reduce bias or escalate |
| Ki=0.1 drift rate | < 1°/min | < 3°/min | Check for oscillation |
| Ki>0 final error | < 10° | < 20° at 30s | Escalate if > 30° |

**Recommended approach:**
1. Start with 0.005 rad/s bias (milder). Verify Ki=0 drifts, Ki=0.05 bounds.
2. If that works, try 0.01 rad/s.
3. If Ki=0.05 cannot bound 0.01 rad/s bias, document the bias-vs-Ki tradeoff
   and report it as a tuning finding.
4. Record actual drift rates for all configs — these are the project's first
   real Mahony tuning data.

**Escalation:** If Ki=0.05 with 0.005 rad/s bias produces drift > 10°/min,
the integral feedback in MahonyAHRS is not working. Check that `integralX_`
accumulates and is applied to the gyro correction. This is a SensorFusion
bug.

---

### A6: Two-Node Joint Angle Recovery

**Status:** Executable now. Two independent harness instances.

**Likely outcome:** Should work well for simple hinge motion. Each node
independently converges to its orientation. Relative quaternion gives joint
angle.

**Predicted failure points:**
- **Heading disagreement:** Two nodes start with independent Mahony filters
  both at identity. If both are static at known orientations, both should
  converge to the correct orientation independently. However, if one node's
  mag correction converges faster than the other, there may be a transient
  heading offset between them.
- **Non-planar rotation:** The test uses flexion around X axis. Cross-axis
  coupling in the filter may introduce small off-axis error into the relative
  quaternion.

**Intermediate thresholds:**

| Threshold | Target | Intermediate | Stop tightening when |
|-----------|--------|-------------|---------------------|
| Joint angle error | < 5° | < 10° | Each node's individual error compounds |
| Max across poses | < 7° | < 15° | 0° pose may have highest error (numerical) |

**Recommended approach:**
1. Start with 90° and 60° angles (largest signal, easiest to validate).
2. Add 30° and 120°.
3. Add 0° last (hardest — small signal, relative error dominates).
4. Allow 5s convergence per node before comparing.

**Escalation:** If joint angle error > 15° at 90° flexion with clean sensors
after 5s convergence, one of the nodes is not converging correctly. Test each
node independently first.

---

## Recommended Codex Task Order (after scripted yaw)

1. **A5 (Ki bias rejection)** — highest value, validates the core Mahony
   mechanism. Quick to implement, clear pass/fail signal.
2. **A1 (static multi-pose)** — identity first, then add poses incrementally.
   Establishes baseline accuracy numbers.
3. **A3 (60s drift)** — depends on A5 establishing that Ki works. Use Ki=0.02.
4. **A2 (dynamic tracking)** — extends existing yaw work to pitch/roll.
5. **A4 (Kp/Ki sweep)** — parameterized test, most complex. Benefits from A1-A3
   establishing what "normal" looks like.
6. **A6 (two-node joint angle)** — independent of filter tuning. Can be done
   anytime but benefits from A1 confirming single-node accuracy.

---

## Escalation Decision Tree

```
Is the error NaN or infinite?
  YES → SensorFusion bug. Check quaternion normalization in MahonyAHRS.
  NO  ↓

Is the error > 45° after 5s convergence with clean sensors?
  YES → Filter is not converging. Check:
        1. Is pipeline.step() receiving correct sensor data? (log accel/gyro/mag)
        2. Is the Mahony dt matching the step interval? (both should be 0.02s)
        3. Is the gimbal syncing to sensors before pipeline.step()?
  NO  ↓

Is the error > intermediate threshold but < 45°?
  YES → This is likely a tuning limitation, not a bug.
        Document the actual value.
        Try adjusting Kp or Ki.
        If Kp=5.0 still cannot converge, escalate to architecture.
  NO  ↓

Is the error > target threshold but < intermediate?
  YES → Accept with the intermediate threshold for now.
        File as a known tuning gap.
        Do NOT fake the target threshold by weakening the test.
  NO  ↓

Test passes target. Ship it.
```

---

## Where to Stop Tightening

**Stop tightening and document instead when:**

1. **Kp=1.0 static accuracy is between 3-8°** — this is likely the Mahony
   floor at default gains. Tightening requires Kp tuning or Ki, not test
   changes.

2. **Dynamic tracking at 30°/s is between 8-15°** — this is the
   complementary filter tradeoff. Gyro tracks fast motion, accel/mag correct
   drift. The lag between them creates tracking error proportional to Kp and
   rotation rate.

3. **60s drift with Ki=0.02 is between 2-5°/min** — this is the Ki-bias
   tradeoff. Tighter drift needs higher Ki, which may cause oscillation.
   Document the Ki-vs-drift curve instead of forcing a threshold.

4. **Convergence time at Kp=1.0 from 90° yaw is 3-5 seconds** — this is
   expected for mag-only heading correction. Tightening requires higher Kp
   or better mag field model.

**Rule:** If a threshold cannot be met with clean sensors at Kp=1.0/Ki=0.02,
it is a filter tuning issue, not an implementation bug. Document the actual
achieved values and move on. The project will revisit filter tuning when
hardware data is available.
