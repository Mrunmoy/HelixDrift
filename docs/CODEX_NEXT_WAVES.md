# Codex Execution Sequencing — Next 2 Waves

**Date:** 2026-03-29
**Author:** Claude / Systems Architect + Review Board
**Inputs:** Sprint 2 report, Codex harness hardening (23cd2ed), Kimi adversarial review + RF/sync spec + mag risk spec

---

## Current State Summary

- **218+ tests passing** (Codex tip: 23cd2ed)
- Harness hardened: configurable Kp/Ki, setSeed(), RunResult, assert on lastFrame()
- All Sprint 2 signoff conditions met
- M1 ~85%, M2 ~40%, M3 ~20%
- Kimi delivered 3 future-looking specs (adversarial review, RF/sync sim, mag calibration risk)

---

## Review of Codex Harness Hardening (23cd2ed)

**Verdict: APPROVED for merge.**

All three Sprint 2 conditions are satisfied:
- `setSeed()` propagation via `assembly_.setSeed(seed)` — confirmed
- `Config` struct with `sf::MocapNodePipeline::Config pipeline` — confirmed
- `lastFrame()` guarded with `assert(!frames.empty())` + `hasFrames()` accessor — confirmed

Bonus: `NodeRunResult` with `runForDuration()`, `CapturedNodeSample`, and `computeSummary()` added. This matches the interface spec's RunResult concept with minor naming differences (acceptable).

**One remaining note:** `driftRateDegPerMin` is computed as `(last - first) / elapsed`. The interface spec recommends linear regression over the last 50% of samples. Current implementation is simpler and acceptable for now but should be noted as a known simplification.

---

## Immediate Wave (Wave A) — Start Now

These tasks have zero blockers. Codex should execute in this order.

### A1: Static Multi-Pose Orientation Accuracy [HIGH]

**File:** `simulators/tests/test_pose_orientation_accuracy.cpp` (new)
**Owner:** Codex / Fusion
**Validates:** requirements §2.1 — segment orientation < 5 deg RMS static

Setup: 7 static orientations (identity, ±90° yaw, ±45° pitch, ±30° roll). Per orientation: create harness with default config, `initAll()`, set gimbal orientation, `syncToSensors()`, run 250 ticks (5s) with `stepMotionAndTick(20000)` at zero rotation rate. Discard first 50 samples (convergence). Record `angularErrorDeg()` for last 200.

**Acceptance:**
- Per-pose steady-state RMS < 3°
- Overall RMS across 7 poses < 5°
- No individual frame > 10°
- Test passes deterministically (use `setSeed(42)`)

### A2: Dynamic Single-Axis Tracking [HIGH]

**File:** `simulators/tests/test_pose_orientation_accuracy.cpp` (same file)
**Owner:** Codex / Fusion

Setup: Three runs — yaw/pitch/roll at 30°/s (0.5236 rad/s). 50-tick warmup, 500-tick motion window via `stepMotionAndTick(20000)`.

**Acceptance:**
- Per-axis RMS < 8°
- Max error < 20°
- No NaN or infinite values

### A3: Long-Duration Heading Drift (60s) [HIGH]

**File:** `simulators/tests/test_pose_drift.cpp` (new)
**Owner:** Codex / Fusion

Setup: Config with Kp=1.0, Ki=0.02. Clean sensors, `setSeed(42)`. 3000 ticks (60s) stationary.

**Acceptance:**
- Max error over 60s < 10°
- Drift rate < 2°/min
- Error at t=60s < 10°

### A4: Mahony Kp/Ki Convergence Sweep [HIGH]

**File:** `simulators/tests/test_pose_mahony_tuning.cpp` (new)
**Owner:** Codex / Fusion

Setup: Sweep Kp={0.5, 1.0, 2.0, 5.0} × Ki={0.0, 0.02, 0.05}. Per combo: run 50 ticks at identity, snap gimbal to 90° yaw, run 500 ticks recovery. Use `runForDuration()` for summary metrics.

**Acceptance:**
- All configs: convergence to < 5° within 5s
- Kp ≥ 1.0: convergence within 2s
- Kp ≥ 2.0: convergence within 1s
- Final steady-state error < 3° for all configs

### A5: Gyro Bias Rejection via Ki [HIGH]

**File:** `simulators/tests/test_pose_mahony_tuning.cpp` (same file)
**Owner:** Codex / Fusion

Setup: Inject `imuSim().setGyroBias({0.01, 0.0, 0.0})`. Compare Ki={0.0, 0.05, 0.1} all with Kp=1.0. 1500 ticks (30s) stationary.

**Acceptance:**
- Ki=0.0: drift rate > 5°/min (bias NOT compensated — validates the test)
- Ki=0.05: drift rate < 2°/min
- Ki=0.1: drift rate < 1°/min
- Ki>0: final error < 10° at t=30s

### A6: Two-Node Joint Angle Recovery [MEDIUM]

**File:** `simulators/tests/test_pose_joint_angle.cpp` (new)
**Owner:** Codex / Fusion

Setup: Two independent `VirtualMocapNodeHarness` instances. Parent at identity, child at {0, 30, 60, 90, 120}° flexion around X. 250 ticks each. Compute `qRel = qA.conjugate() * qB`, extract angle.

**Acceptance:**
- Joint angle error < 5° for all 5 poses
- No pose > 7°

---

## Deferred Wave (Wave B) — After Wave A completes

### B1: CSV Export + Python Plot Script [MEDIUM]

**Owner:** Codex / Host Tools
**Files:** `simulators/harness/CsvExport.hpp/cpp`, `simulators/scripts/plot_test_run.py`
**Blocked by:** Nothing (can start in parallel with Wave A if Host Tools team is free)

Export `NodeRunResult` to CSV. Python script produces 4 standard plots. Gated by `HELIX_TEST_EXPORT=1`.

**Acceptance:** CSV parseable, PNG output, empty-safe.

### B2: Motion Profile JSON Library [MEDIUM]

**Owner:** Codex / Fusion
**Files:** `simulators/motion_profiles/**/*.json`
**Blocked by:** Nothing

12 canonical profiles. Each must load with `VirtualGimbal::loadMotionScript()`.

### B3: Calibration Effectiveness — Hard Iron [MEDIUM]

**Owner:** Codex / Sensor Validation
**File:** `simulators/tests/test_pose_calibration_effectiveness.cpp`
**Blocked by:** A1 (needs baseline accuracy reference)

Inject hardIron {15, -10, 5} uT. Measure uncalibrated vs. calibrated accuracy. Requires sphere-fit algorithm (~30 lines inline).

**Acceptance:** Baseline RMS > 8°, post-cal RMS < 5°, improvement > 2x.

### B4: Remaining Sensor Validation Matrix Gaps [MEDIUM]

**Owner:** Codex / Sensor Validation
**Blocked by:** Nothing

Per `docs/sensor-validation-matrix.md`:
- LSM6DSO: missing 3 of 6 accel orientations, stationary gyro, multi-axis rotation, accel norm, gyro noise stddev
- BMM350: missing yaw/pitch orientation response tests with tighter tolerances
- LPS22DF: missing cold/hot temp, below-sea-level altitude

**Acceptance:** All sensor-validation-matrix.md criteria covered.

---

## Blocked — Waiting for RF/Sync Work

These tasks depend on infrastructure from Kimi's `RF_SYNC_SIMULATION_SPEC.md`. They should NOT be attempted until the RF/sync sim layer exists.

| Task | Blocked By | When to Start |
|---|---|---|
| VirtualRFMedium implementation | RF/sync spec review + approval | After Wave B |
| VirtualSyncNode / VirtualSyncMaster | VirtualRFMedium | After RF medium works |
| Multi-node sync convergence tests | VirtualSyncNode | After sync node works |
| Network impairment tests | VirtualRFMedium | After RF medium works |

**Recommendation:** Kimi's RF spec is well-structured but represents ~24 hours of Codex work. Defer until Waves A+B are complete and M2 is solidly closed. This is M4 work.

---

## Blocked — Waiting for Magnetic/Calibration Infrastructure

These tasks depend on infrastructure from Kimi's `MAGNETIC_CALIBRATION_RISK_SPEC.md`. They should NOT be attempted now.

| Task | Blocked By | When to Start |
|---|---|---|
| MagneticEnvironment class | Mag spec review + approval | After Wave B, M5 scope |
| CalibratedMagSensor wrapper | MagneticEnvironment | After MagEnv works |
| Disturbance detection tests | CalibratedMagSensor | After cal sensor works |
| AHRS mag rejection tests | MagneticEnvironment + CalibratedMagSensor | After both exist |

**Recommendation:** Kimi's mag spec is ~31 hours of work and belongs to M5-M6. Codex should NOT start this until M2 is closed. The simple hard iron test in B3 is the only mag calibration work appropriate for now — it uses existing simulator error injection without new infrastructure.

---

## What to Merge Now vs Later

### Merge Now

- **23cd2ed** (harness hardening) — all signoff conditions met, no blockers
- Any Wave A test files as they pass — these are additive (new test files only, no modifications to existing code)

### Merge After Wave A Completes

- Full Wave A batch — once all 6 tasks pass, merge as a single integration
- This closes M2 "orientation accuracy validated" acceptance criteria

### Merge Later (Wave B or beyond)

- CSV export, motion profiles, calibration tests — lower urgency, higher risk of scope creep
- RF/sync infrastructure — M4 scope, not current focus
- Mag environment infrastructure — M5-M6 scope

---

## Priority Ladder (single ordered list)

| # | Task | Wave | Milestone | Est. Size |
|---|------|------|-----------|-----------|
| 1 | Static multi-pose accuracy (A1) | A | M2 | Medium |
| 2 | Dynamic single-axis tracking (A2) | A | M2 | Medium |
| 3 | 60s heading drift (A3) | A | M2 | Small |
| 4 | Kp/Ki convergence sweep (A4) | A | M2 | Medium |
| 5 | Gyro bias rejection via Ki (A5) | A | M2 | Medium |
| 6 | Two-node joint angle (A6) | A | M2 | Medium |
| 7 | CSV export + Python plots (B1) | B | M2 evidence | Medium |
| 8 | Motion profile JSONs (B2) | B | M2 | Small |
| 9 | Hard iron cal effectiveness (B3) | B | M1 completion | Medium |
| 10 | Sensor validation matrix gaps (B4) | B | M1 completion | Medium |
| 11+ | RF/sync infrastructure | Deferred | M4 | Large |
| 12+ | Mag environment infrastructure | Deferred | M5-M6 | Large |

---

## Kimi Adversarial Review — Response

Kimi's adversarial review correctly identifies that the simulators idealize many real-world effects (I2C timing, temperature drift, Allan variance, magnetic disturbances, clock drift). These are all valid concerns for hardware transition.

**Claude's position:** These idealizations are acceptable for M1-M2 simulation-first proof. The project goal right now is to prove the algorithms work under controlled conditions. Adding Level C fidelity (temperature, Allan variance, magnetic environment) is M5-M6 work and should not derail the current focus on closing M2.

The Kimi specs (RF/sync, mag risk) are well-prepared and should be queued for implementation after M2 closes — not interleaved with current work.
