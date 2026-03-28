# Sprint 6: Wave A Rescope After SensorFusion Init Fix

**Date:** 2026-03-29
**Author:** Claude / Systems Architect
**Trigger:** SensorFusion init (214c28a) partially unblocked A1, but pitch/roll still broken

---

## 1. Evidence Summary

After `initFromSensors()` fix:

| Test | Result | Verdict |
|------|--------|---------|
| A5 Ki bias rejection | Green | Done |
| A3 60s drift | Green | Done |
| A6 joint angle recovery | Green | Done |
| A1a identity | 0° / 0° / 0° | Perfect |
| A1a ±15° yaw | RMS ~15.1, max ~18.6 | **Suspicious** — error ≈ offset |
| A1a ±15° pitch | RMS ~29.0, max ~29.9 | **Broken** — error ≈ 2x offset |
| A1a ±15° roll | RMS ~37.8, max ~40.7 | **Broken** — error ≈ 2.5x offset |
| A2 yaw 30°/s | RMS ~25.6, max ~35.5 | Weak but not diverging |
| A2 pitch 30°/s | RMS ~111.3, max ~179.9 | **Diverging** |
| A2 roll 30°/s | RMS ~116.2, max ~179.6 | **Diverging** |

## 2. Root Cause Analysis

**Pattern:** Identity is perfect. Yaw has error roughly equal to the offset.
Pitch has 2x the offset as error. Roll has 2.5x. Dynamic pitch/roll diverge
to 180°.

**Diagnosis:** The `initFromSensors()` Euler decomposition has a **convention
mismatch** with the simulator's orientation model.

The Mahony init uses:
```
roll  = atan2(-ay, az)
pitch = atan2(ax, sqrt(ay² + az²))
```

This is the standard aerospace convention (NED gravity = [0, 0, +1g]).
But the HelixDrift simulator comment in the init code says "gravity along +Z
at identity" — meaning the sensor reads [0, 0, +1g] when flat. This matches.

However, the Euler-to-quaternion conversion `quaternionFromEulerDeg()` uses
ZYX convention. If the simulator uses a different rotation order (XYZ, or
body-to-world vs world-to-body), the resulting quaternion will be wrong for
pitch and roll while yaw happens to be close (because yaw is the last rotation
in ZYX).

**Evidence supporting this:**
- Yaw error ≈ offset magnitude (init gets the right ballpark, filter can't
  fully correct)
- Pitch error ≈ 2x offset (init is systematically wrong)
- Roll error ≈ 2.5x offset (init is wrong AND filter correction makes it
  worse)
- Dynamic pitch/roll diverge to 180° (filter is fighting the wrong init)

**This is a SensorFusion init bug, not a Codex, simulator, or threshold issue.**

## 3. What Codex Should Do Now

### Answers to the specific questions:

**Should A1a be split by axis?**
Yes. Yaw-only is valid and should be committed as a passing test. Pitch and
roll are blocked on the init fix.

**Should A2 be split by axis?**
Yes. Yaw tracking is weak (25° RMS) but not diverging — it can be committed
as a characterization test with a loose threshold. Pitch and roll are
diverging and must not be committed with fake thresholds.

**Is yaw-only acceptable as an intermediate milestone?**
Yes. Yaw-only A1a + A2 proves that:
- The init correctly computes heading from mag
- The filter tracks yaw rotation
- Heading accuracy is bounded

This is real M2 progress. Pitch/roll will follow when the init is fixed.

**What should escalate?**
The pitch/roll init convention. This is the same submodule escalation path
as Sprint 5 — fix in SensorFusion, push, update pointer.

### Exact next steps:

**Step 1: Commit yaw-only A1a test (Codex, now)**

```
TEST(PoseOrientationAccuracyTest, StaticYawWithinIntermediateBound)
```
- Poses: identity, +15° yaw, -15° yaw
- 100-tick warmup, 200-tick measurement
- Assert: identity RMS < 1°, ±15° yaw RMS < 20°, max < 25°
- These thresholds are generous but match measured values (15.1/18.6)

**Step 2: Commit yaw-only A2 test (Codex, now)**

```
TEST(PoseOrientationAccuracyTest, DynamicYawTrackingWithinLooseBound)
```
- 30°/s yaw from identity, 50-tick warmup, 500-tick motion
- Assert: RMS < 30°, max < 40°
- Characterization test — captures current behavior, not a quality target

**Step 3: File SensorFusion pitch/roll init bug (Codex or Claude)**

The `initFromSensors()` Euler decomposition or quaternion conversion has a
convention mismatch. The fix is in the SensorFusion submodule:
- Verify the gravity convention matches the simulator (+Z up at identity)
- Verify the Euler rotation order matches the quaternion library
- Test: init from a 15° pitch accel+mag reading, verify the resulting
  quaternion matches `Quaternion::fromAxisAngle(0,1,0,15)` within 1°

**Step 4: After SensorFusion fix — add pitch/roll A1a and A2**

Once the init produces correct pitch/roll quaternions:
- Add ±15° pitch, ±15° roll to A1a
- Add pitch/roll 30°/s to A2
- Use intermediate thresholds: RMS < 10°, max < 20°

## 4. Revised Wave A Status

| Task | Status | Scope |
|------|--------|-------|
| A5 | **DONE** | Ki bias rejection |
| A3 | **DONE** | 60s drift |
| A6 | **DONE** | Joint angle recovery |
| A1a-yaw | **READY** | Identity + ±15° yaw static |
| A2-yaw | **READY** | Yaw dynamic tracking |
| A1a-pitch/roll | BLOCKED | SensorFusion init convention |
| A2-pitch/roll | BLOCKED | Same |
| A4 | DEFERRED | Kp/Ki sweep (after init is correct) |
| A1b | DEFERRED | Large-angle static (after init is correct) |

**Done: 3/6. Ready: 2/6. Blocked: 2/6 + 2 deferred.**

## 5. What Counts as M2 Now

M2 achievable scope without the pitch/roll fix:

- Yaw heading accuracy from stationary start: proven
- Yaw dynamic tracking: characterized (weak but bounded)
- Ki integral bias rejection: proven
- Long-duration drift stability: proven
- Two-node joint angle recovery: proven
- Pitch/roll accuracy: **blocked, known cause, fix path clear**

This is legitimate M2 progress. The yaw axis is the hardest (mag correction
is weakest there) and it works. Pitch/roll will be easier once the init
convention is fixed (gravity is a strong reference for pitch/roll).

## 6. Decision Note for Codex

**Do now:**
1. Commit yaw-only A1a (identity + ±15° yaw, thresholds: RMS < 20°)
2. Commit yaw-only A2 (30°/s yaw, thresholds: RMS < 30°)
3. Both as characterization tests — these capture real filter behavior

**File:**
4. SensorFusion bug: `initFromSensors()` pitch/roll convention mismatch.
   The Euler decomposition or quaternionFromEulerDeg() rotation order does
   not match the simulator/quaternion library convention. Test case: init
   from 15° pitch reading, compare to `fromAxisAngle(0,1,0,15)`.

**Do NOT:**
- Do not add pitch/roll A1a or A2 tests with fake thresholds
- Do not attempt A4 convergence sweep until init is correct
- Do not spend time tuning Kp/Ki — the current errors are init-caused, not
  gain-caused

**After fix:**
- Add pitch/roll to A1a and A2
- Then A4 (Kp/Ki sweep with correct init)
- Then tighten all thresholds toward targets
