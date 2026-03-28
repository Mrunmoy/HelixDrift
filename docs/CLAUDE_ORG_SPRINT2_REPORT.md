# Claude Org Sprint 2 Report

**Date:** 2026-03-29
**Trigger:** Codex completed implementation wave (218/218 tests)
**Subteams:** Review Board, Systems Architect, Pose Inference

---

## 1. Review Board Findings

### PASS Items

- **SimMetrics.hpp** — Correct geodesic error. Handles double-cover via `std::abs(dot)`, clamps to [0,1]. All 4 spec tests present.
- **VirtualSensorAssembly.hpp** — Correct dual-bus wiring, safe init order (unique_ptrs before references), clean `initAll()` short-circuit.
- **VirtualMocapNodeHarness.hpp** — Correctly composes assembly + pipeline + MocapNodeLoop + TimestampSynchronizedTransport. VirtualClock, CaptureTransport, AnchorQueue, OffsetSyncFilter are minimal and testable.
- **test_lsm6dso_simulator.cpp** — Strongest file: 20 tests covering WHO_AM_I, register R/W, accel gravity (3 orientations), gyro (2 axes), temperature, bias/scale injection, noise stats, deterministic seeding, bulk read, full-scale tracking, sequential consistency.
- **test_lps22df_simulator.cpp** — 15 tests covering identity, register R/W, software reset, sea-level pressure, altitude, temperature, overrides, bias, noise, seeding, multi-register read, base pressure config.
- **test_sensor_fusion_integration.cpp** — Clean refactor using VirtualSensorAssembly. Eliminates boilerplate, preserves semantics.
- **CMakeLists.txt** — All new files properly wired, include paths correct.
- **PER_SENSOR_VALIDATION_MATRIX.md** — Consistent with Claude's sensor-validation-matrix.md.
- **pose-inference-recommendation.md** (Kimi) — Aligned with Claude's requirements doc, adds product framing.

### FINDINGS

#### BLOCKER

**B1: `lastFrame()` has undefined behavior on empty frames vector**
- `VirtualMocapNodeHarness.hpp:111` — `.back()` on empty vector is UB. No guard. A future test author could easily trigger this.
- **Fix:** Add `assert(!captureTransport_.frames.empty())` or return `std::optional`.

#### WARNING

**W1: Naming convention conflict** — Global CLAUDE.md says `m_` prefix; project CLAUDE.md says trailing `_`. All new code uses trailing `_` (matches project convention). Noting for awareness only.

**W2: No `setSeed()` propagation in harness** — Interface spec requires deterministic seeding. Harness never calls `setSeed()`. Tests without noise work today but noise-injection tests will be non-deterministic.

**W3: `ConstantYawMotion` tolerance is 35°** — Very loose for 0.5s of slow yaw. Acceptable as smoke test, not as accuracy proof.

**W4: `StaticQuarterTurnConverges` tolerance is 55°** — 45° initial error should converge tighter than 55° after 20 steps. Should be documented if this covers a known fusion weakness.

**W5: `FullRotation360DegreesReturnsToStart` asserts `dot > 0.0`** — This passes for ANY non-zero quaternion. The dot product would only be 0.0 for a perfect 180° error. Test provides no validation value.
- **Fix:** Should be `EXPECT_GT(std::abs(dot), 0.7f)` or similar.

#### NOTE

**N1:** `angularErrorDeg` may return NaN for NaN inputs (std::clamp is impl-defined for NaN). Spec says "never returns NaN". Low practical risk.

**N2:** Include order in new headers conflicts between global and project CLAUDE.md conventions. New code follows global convention (project headers first).

**N3:** `VirtualSensorAssembly` default addresses derived from two separate sources (simulator header + driver config). Silent breakage if either changes.

### Missing Coverage (against sensor-validation-matrix.md)

- LSM6DSO: missing multi-axis rotation, stationary gyro, 3 of 6 accel orientation poses, accel norm check, gyro noise stddev match
- BMM350: no new tests in this wave
- LPS22DF: missing cold/hot temp test, below-sea-level altitude
- Harness: missing transport failure test, multiple anchor test, `advanceTimeUs()` test

### Signoff: READY WITH CONDITIONS

1. **Must fix B1:** Guard `lastFrame()` against empty vector
2. **Should fix W5:** 360° return-to-start assertion is trivially weak
3. **Should track W2:** Add deterministic seeding before noise-injection tests

---

## 2. Systems Architect Milestone Update

### Milestone Status

| Milestone | Status | Progress | Key Evidence |
|---|---|---|---|
| **M1: Per-Sensor Proof** | **~85% complete** | 55 sensor-level tests | Identity, register, physical response, basic error injection, deterministic seeding all done. Gaps: full calibration recovery tests (6-position tumble, sphere fit, noise characterization). |
| **M2: Single-Node Assembly** | **~40% complete** | Infrastructure done, validation pending | VirtualSensorAssembly + VirtualMocapNodeHarness + SimMetrics delivered. Basic pose quality asserted. Gaps: motion script regression, convergence/drift tests, Mahony tuning, error sweeps. |
| **M3: Host-Side Node Runtime** | **~20% started** | Harness covers MocapNodeLoop + transport | VirtualMocapNodeHarness integrates MocapNodeLoop, timestamp sync, capture transport. Cadence and anchor tests pass. Gaps: health frame capture, pipeline failure tests, dropped sample handling. |
| **M4-M7** | Not started | — | Expected |

### What Changed This Wave

- Test count: 207 → 218 (+11 tests)
- New infrastructure: VirtualSensorAssembly, VirtualMocapNodeHarness, SimMetrics
- Existing integration tests refactored to use assembly harness
- Per-sensor proof tightened: full-scale tracking, software reset tests added
- Three design docs reviewed and aligned (Claude + Kimi pose docs)

### Recommended Next Wave Priorities

**Priority 1 (unblocks everything):** Harden the harness
- Add `setSeed()` propagation
- Add configurable `MocapNodePipeline::Config` (Kp/Ki)
- Add `RunResult` aggregation with RMS/max/drift metrics

**Priority 2 (M2 core validation):** Orientation accuracy tests
- Static accuracy at multiple orientations (target: < 5 deg RMS)
- Convergence from initial error (Kp sweep)
- Ki bias rejection test
- Long-duration drift test (60s stationary)

**Priority 3 (M1 completion):** Calibration recovery tests
- Accel 6-position tumble (W2-B)
- Gyro rate scale calibration (W2-C)
- Mag hard iron sphere fit (W2-D)

**Priority 4 (M2 evidence):** Motion scripts + CSV export
- Create canonical motion profiles
- Implement CSV export
- Python plotting script

---

## 3. Pose Inference Experiment Plan

7 experiments for Codex, ordered by priority. Based on pose-inference-requirements.md targets and the harness that now exists.

### Prerequisite: P0 — Add pipeline config to VirtualMocapNodeHarness

Currently the harness always constructs `MocapNodePipeline` with default config (Kp=1.0, Ki=0.0). Add an optional `sf::MocapNodePipeline::Config` parameter. ~5 lines of change. Blocks experiments 3-5.

### Experiment 1: Static Orientation Accuracy (Multi-Pose) [HIGH]

**File:** `simulators/tests/test_pose_orientation_accuracy.cpp`
**Validates:** Per-segment orientation < 5 deg RMS (requirements §2.1)
**Setup:** 7 static orientations (identity, ±90° yaw, ±45° pitch, ±30° roll). 250 ticks (5s) each, discard first 50 for convergence.
**Assertions:** Per-pose steady-state RMS < 3°, overall RMS < 5°, no frame > 10°.
**Dependencies:** None.

### Experiment 2: Dynamic Single-Axis Tracking [HIGH]

**File:** `simulators/tests/test_pose_orientation_accuracy.cpp` (same file)
**Validates:** < 10 deg RMS under motion (requirements §2.1)
**Setup:** Three runs: yaw/pitch/roll at 30°/s (0.5236 rad/s). 50-tick warmup, 500-tick motion window.
**Assertions:** Per-axis RMS < 8°, max < 20°, no NaN.
**Dependencies:** None.

### Experiment 3: Mahony Kp/Ki Convergence Sweep [HIGH]

**File:** `simulators/tests/test_pose_mahony_tuning.cpp`
**Validates:** Filter tuning convergence speed (brainstorm §5a)
**Setup:** Kp={0.5, 1.0, 2.0, 5.0} × Ki={0.0, 0.02, 0.05}. Snap gimbal 90° after 50-tick settle. 500 ticks recovery.
**Assertions:** All configs T5 < 5s. Kp≥1: T5 < 2s. Kp≥2: T5 < 1s. Steady-state < 3°.
**Dependencies:** P0.

### Experiment 4: Gyro Bias Rejection via Ki [HIGH]

**File:** `simulators/tests/test_pose_mahony_tuning.cpp` (same file)
**Validates:** Ki integral compensates gyro bias (brainstorm §5c — THE key test)
**Setup:** Inject 0.01 rad/s gyro bias. Compare Ki={0.0, 0.05, 0.1} with Kp=1.0. 1500 ticks (30s).
**Assertions:** Ki=0: drift > 5°/min. Ki=0.05: drift < 2°/min. Ki=0.1: drift < 1°/min. Ki>0: final error < 10°.
**Dependencies:** P0.

### Experiment 5: Long-Duration Heading Drift (60s) [HIGH]

**File:** `simulators/tests/test_pose_drift.cpp`
**Validates:** < 10° drift over 60s (requirements §2.1)
**Setup:** Kp=1.0, Ki=0.02. Clean sensors. 3000 ticks (60s) stationary.
**Assertions:** Max error < 10°, drift rate < 2°/min.
**Dependencies:** P0.

### Experiment 6: Two-Node Joint Angle Accuracy [MEDIUM]

**File:** `simulators/tests/test_pose_joint_angle.cpp`
**Validates:** Joint angle < 5 deg (requirements §2.2)
**Setup:** Two independent harness instances. Parent at identity, child at {0, 30, 60, 90, 120}° flexion. 250 ticks each.
**Computation:** `qRel = qA.conjugate() * qB`, `angle = 2 * acos(|qRel.w|) * 180/pi`
**Assertions:** Error < 5° all poses, no pose > 7°.
**Dependencies:** None (two separate instances).

### Experiment 7: Calibration Effectiveness — Hard Iron [MEDIUM]

**File:** `simulators/tests/test_pose_calibration_effectiveness.cpp`
**Validates:** Calibration improves accuracy (brainstorm §4a)
**Setup:** Inject hardIron {15, -10, 5} uT. Measure baseline (uncalibrated) vs post-calibration accuracy.
**Assertions:** Baseline RMS > 8°, post-cal RMS < 5°, improvement > 2x.
**Dependencies:** May need mag correction API.

### Implementation order: P0 → Exp 1 → Exp 2 → Exp 5 → Exp 3 → Exp 4 → Exp 6 → Exp 7
Experiments 1, 2, 6 can start immediately. Experiments 3-5 blocked on P0.

---

## 4. Consolidated Claude Recommendation for Codex Next Steps

### Immediate (next wave)

1. **Harden VirtualMocapNodeHarness:** Add `setSeed()` propagation, configurable `MocapNodePipeline::Config`, and `RunResult` aggregation. This is small (~50 lines) and unblocks all orientation accuracy and Mahony tuning work.

2. **Implement Experiments 1-3:** Static accuracy, convergence from error, and Ki bias rejection. These directly validate the v1 pose-inference-requirements targets and are the highest-value tests not yet in the suite.

3. **Add one tighter static accuracy assertion** to the existing harness test suite (< 10 deg, replacing the current 15 deg threshold in a new test — do not modify existing test).

### Next wave after that

4. **Implement Experiments 4-6:** Dynamic tracking, two-node joint angle, heading stability.
5. **Complete M1 calibration tests:** Accel tumble (W2-B), gyro rate scale (W2-C), mag sphere fit (W2-D).
6. **CSV export + motion profiles + Python plot script** (Host Tools team).

### Deferred

- M3 completion (health frames, pipeline failure, dropped samples)
- M4-M7 (multi-node sync, kinematics, platform port)
- Multi-node simulation (needs M4 infrastructure)

---

## 5. Updated Claude Org Status

Sprint 2 complete. All three subteam outputs consolidated above.

| Subteam | Output | Status |
|---|---|---|
| Review Board | Code review findings, signoff recommendation | Complete |
| Systems Architect | Milestone update, next-wave priorities | Complete |
| Pose Inference | 6 experiment specs for Codex | Complete |

**Next Claude org activation:** When Codex delivers the hardened harness and Experiments 1-3, activate Review Board for the next signoff cycle.
