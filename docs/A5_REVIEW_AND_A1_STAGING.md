# A5 Review and A1 Staging Instructions

**Sprint:** Claude Org Sprint 4 (continued)
**Codex commit:** f9295c6 `codex: add mahony bias rejection tests`

---

## Part 1: A5 Review

### What was delivered

Two tests in `test_pose_mahony_tuning.cpp`:
1. `GyroZBiasWithoutIntegralFeedbackShowsPositiveHeadingDrift` — Ki=0, bias=0.01 rad/s, asserts drift IS present
2. `IntegralFeedbackReducesHeadingErrorFromGyroZBias` — compares Ki=0 vs Ki=0.05 vs Ki=0.1, asserts monotonic improvement

Supporting infrastructure (prior commits):
- `runWithWarmup(warmupSteps, measuredSteps, stepUs)` — discards warmup, records measured
- `linearDriftRateDegPerMin(errors, samplePeriodUs, trailingFraction)` — proper least-squares linear regression over trailing 50%
- `summarizeErrorSeriesDeg()`, `firstIndexAtOrBelowDeg()` — reusable helpers

### Alignment with acceptance guide

| Guide criterion | Test implementation | Verdict |
|---|---|---|
| Start with 0.005 rad/s bias | Used 0.01 rad/s | Acceptable — guide said try 0.005 first, but 0.01 works fine since Ki bounds it |
| Ki=0 must show drift > 5°/min | Asserts `driftRate > 0.5` and `finalError > 3` | **Softer than guide** — 0.5°/min is weaker than 5°/min. See finding W1. |
| Ki=0.05: drift < 2°/min | Asserts `finalError < 2.0` and `maxError < 3.0` | **Good** — uses absolute error bounds, not drift rate. Equivalent. |
| Ki=0.1: drift < 1°/min | Asserts `finalError < 0.5` and `maxError < 2.5` | **Tight and good.** |
| Ki>0: final error < 10° | Ki=0.05 final < 2°, Ki=0.1 final < 0.5° | Exceeds guide target. |
| Monotonic improvement across Ki values | `ki005 < noIntegral`, `ki01 < ki005` for both final and RMS | **Correct pattern.** |
| Deterministic seeding | `setSeed(42)` | Present. |
| Uses runWithWarmup | 50 warmup, 1500 measured | **Good.** 50 ticks = 1s warmup, 1500 ticks = 30s measurement. |
| linearDriftRateDegPerMin | Used with trailing 50% | **Correct.** Uses the proper linear regression, not the simpler (last-first)/elapsed. |

### Findings

**PASS:**
- Test structure is clean. Helper function `runStationaryBiasCase()` is reusable for future Kp/Ki sweep work (A4).
- The `linearDriftRateDegPerMin` implementation is better than what the harness has internally — uses proper least-squares with configurable trailing fraction. The harness's `computeSummary()` still uses the simpler (last-first)/elapsed, but the test bypasses it correctly.
- Both tests use deterministic seeding.
- CMake wiring is correct.
- The monotonic comparison (`ki005 < noIntegral`, `ki01 < ki005`) is the right pattern — proves the Ki mechanism works directionally, not just against absolute thresholds.

**W1 (WARNING): Ki=0 drift assertion is too soft.**
The guide recommends asserting drift > 5°/min for 0.01 rad/s bias. The test asserts `driftRate > 0.5` (0.5°/min). With 0.01 rad/s = 0.57°/s = 34°/min theoretical drift, the actual drift should be much higher than 0.5°/min. The assertion is essentially "drift exists" rather than "drift is substantial."

**Recommendation:** Tighten to `EXPECT_GT(baseline.driftRateDegPerMin, 5.0f)` in a follow-up. Not blocking — the test already proves Ki=0 drifts and Ki>0 bounds it.

**W2 (WARNING): No explicit drift-rate assertion on Ki>0 cases.**
The test asserts absolute error bounds (finalError, maxError) but not drift rate for Ki=0.05 and Ki=0.1. The guide specifically calls for `Ki=0.05: drift < 2°/min`. The absolute bounds are sufficient to prove the mechanism works, but drift rate is a more meaningful metric for tuning documentation.

**Recommendation:** Add `EXPECT_LT(ki005.driftRateDegPerMin, 2.0f)` in a follow-up. Not blocking.

**N1 (NOTE): `EXPECT_TRUE` vs `ASSERT_TRUE` for initAll.**
The helper uses `EXPECT_TRUE(harness.initAll())` instead of `ASSERT_TRUE`. If init fails, the test continues with an uninitialized harness and will produce confusing failures. Should be `ASSERT_TRUE` in a follow-up, or the function should return early on failure.

### Verdict

**APPROVE WITH FOLLOW-UPS.**

The A5 implementation is solid. It validates the core Mahony Ki mechanism with clean structure, proper regression form, and deterministic seeding. The findings are tightening suggestions, not correctness issues.

Follow-ups (can be batched with A1 or later):
1. Tighten Ki=0 drift assertion to > 5°/min
2. Add explicit drift-rate assertions for Ki=0.05 and Ki=0.1
3. Change `EXPECT_TRUE(harness.initAll())` to `ASSERT_TRUE` in helper

---

## Part 2: A1 Staging Instructions for Codex

### File

`simulators/tests/test_pose_orientation_accuracy.cpp` (new file)

### Pose order — start with these 3

| # | Pose | Gimbal orientation | Why first |
|---|------|-------------------|-----------|
| 1 | Identity (Z up) | `{1, 0, 0, 0}` | Filter starts at identity — should be trivially accurate |
| 2 | 90° yaw | `Quaternion::fromAxisAngle(0, 0, 1, 90.0f)` | Pure heading change, tests mag correction |
| 3 | -90° yaw | `Quaternion::fromAxisAngle(0, 0, 1, -90.0f)` | Symmetric check |

If these 3 pass, add:

| 4 | +45° pitch | `Quaternion::fromAxisAngle(0, 1, 0, 45.0f)` | Tests accel-gravity correction |
| 5 | -45° pitch | `Quaternion::fromAxisAngle(0, 1, 0, -45.0f)` | Symmetric |
| 6 | +30° roll | `Quaternion::fromAxisAngle(1, 0, 0, 30.0f)` | Roll axis |
| 7 | -30° roll | `Quaternion::fromAxisAngle(1, 0, 0, -30.0f)` | Symmetric |

### Test structure

Reuse the pattern from A5: helper function that creates a harness, sets pose, runs, returns metrics.

```
helper function: runStaticPose(Quaternion targetOrientation) -> ErrorSeriesStats
  1. create harness with default config (Kp=1.0, Ki=0.0)
  2. setSeed(42)
  3. initAll(), resetAndSync()
  4. gimbal().setOrientation(target)
  5. gimbal().syncToSensors()
  6. runWithWarmup(warmupSteps=100, measuredSteps=200, stepUs=20000)
     (100 ticks = 2s warmup, 200 ticks = 4s measurement)
  7. collect errors from result.samples into vector
  8. return summarizeErrorSeriesDeg(errors)
```

### Thresholds — use intermediate first

**First commit (3 poses):**

| Assertion | Threshold | Rationale |
|---|---|---|
| Identity RMS | < 5° | Filter starts aligned, should be < 1° but use 5° buffer |
| ±90° yaw RMS | < 8° | Mag correction from 90° may be slower |
| Per-pose max | < 15° | Transient convergence |

**Second commit (7 poses):**

| Assertion | Threshold | Rationale |
|---|---|---|
| Per-pose RMS | < 8° | Intermediate from guide |
| Overall RMS across 7 | < 10° | Intermediate from guide |
| Per-pose max | < 20° | Intermediate from guide |

**Tighten later (only if intermediate passes):**

| Assertion | Target | Only tighten when |
|---|---|---|
| Per-pose RMS | < 3° | After tuning Kp or adding Ki |
| Overall RMS | < 5° | After all 7 poses pass at < 8° |
| Per-pose max | < 10° | After warmup period is sufficient |

### What to defer

- **Do NOT add Ki for A1.** Use Ki=0.0. A1 tests static accuracy with proportional-only correction. Ki is for drift correction (tested in A5). Mixing them confuses the diagnostic.
- **Do NOT test ±90° pitch.** Near-gimbal-lock behavior is a known Mahony issue. Save for a separate stress test later.
- **Do NOT add noise injection.** A1 is clean-sensor accuracy. Noise injection is A4/A5 territory.

### Escalation criteria

- **Identity RMS > 5° after 2s warmup with clean sensors:** Something is wrong. Check that `pipeline.step()` receives correct accel/gyro/mag data at the target orientation. Log the raw sensor values and the filter quaternion.
- **±90° yaw RMS > 15° after 2s warmup:** Mag correction may be too weak at Kp=1.0. Try Kp=2.0. If Kp=2.0 fixes it, this is a tuning finding. If Kp=5.0 still exceeds 15°, escalate.
- **Any NaN or infinite value:** SensorFusion bug. Stop and report.
- **Pitch/roll poses dramatically worse than yaw (> 2x error):** Unexpected — accel correction should be strong for pitch/roll. Investigate the accel data at those orientations.

### Warmup duration rationale

100 ticks = 2 seconds. The guide estimates convergence from 90° at Kp=1.0 takes 2-4 seconds. Using 100 ticks gives the filter 2 seconds to settle, which should be sufficient for yaw and generous for pitch/roll (which converge faster via accel). If ±90° yaw still shows high error after 2s warmup, increase to 150 ticks (3s) before tightening thresholds.

### Summary for Codex

1. Create `test_pose_orientation_accuracy.cpp`
2. Write `runStaticPose(Quaternion)` helper returning `ErrorSeriesStats`
3. First test: 3 poses (identity, ±90° yaw), intermediate thresholds (RMS < 8°, max < 15°)
4. If green: add 4 more poses (±45° pitch, ±30° roll), same thresholds
5. If green: add overall RMS assertion across all 7 (< 10°)
6. Do NOT tighten below intermediate thresholds in this commit
7. Do NOT add Ki, noise, or ±90° pitch
