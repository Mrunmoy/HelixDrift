# Simulation Testing — Unified Execution Plan

This document reconciles the wave-based parallelizable task breakdown with
the milestone-based simulation backlog and maps each task to its owning
Codex team.

Design documents referenced:
- `docs/sensor-validation-matrix.md` — acceptance criteria for Milestone 1
- `docs/simulation-harness-interface.md` — interface contract for Wave 2
- `docs/SIMULATION_BACKLOG.md` — milestone roadmap (Milestones 1-7)
- `simulators/docs/CALIBRATION_SIM_BRAINSTORM.md` — detailed test procedures

---

## Milestone-to-Wave Mapping

| Backlog Milestone | Wave Tasks | Primary Codex Team |
|-------------------|-----------|-------------------|
| M1: Per-Sensor Proof | W1-B, W1-D, W2-B, W2-C, W2-D | Sensor Validation |
| M2: Single-Node Assembly | W1-A, W1-C, W1-E, W2-A, W3-A, W3-B, W3-C | Fusion And Kinematics |
| M2 (evidence) | W1-F, W3-D, W4-A | Host Tools And Evidence |
| M3-M7 | Not yet broken into tasks | Deferred |

---

## Wave 1 — No Dependencies (all start immediately)

All Wave 1 tasks are independent and can be assigned to separate agents.

### W1-A: Angular Error Metric Utility [Small]

**Owner:** Codex / Fusion And Kinematics
**Milestone:** M2 infrastructure
**Interface spec:** `docs/simulation-harness-interface.md` (Angular Error Function section)

Create `simulators/harness/SimMetrics.hpp` and `.cpp` with `angularErrorDeg()`
and `fromAxisAngleDeg()`. Add `simulators/tests/test_sim_metrics.cpp`.

Acceptance: 5+ unit tests pass. See interface spec for exact function contract.

---

### W1-B: Deterministic Seeding for Simulators [Small]

**Owner:** Codex / Sensor Validation
**Milestone:** M1 infrastructure
**Status:** ALREADY DONE — `setSeed()` exists on all three simulators.

Verify: two runs with same seed produce identical data. If already passing,
mark complete and move on.

---

### W1-C: CSV Export Module [Small]

**Owner:** Codex / Host Tools And Evidence
**Milestone:** M2 infrastructure
**Interface spec:** `docs/simulation-harness-interface.md` (CSV Export section)

Create `simulators/harness/SimTypes.hpp`, `CsvExport.hpp`, `CsvExport.cpp`.
See interface spec for exact CSV schema and `shouldExport()` behavior.

---

### W1-D: Lsm6dsoSimulator — Add setLinearAcceleration [Small]

**Owner:** Codex / Sensor Validation
**Milestone:** M1 enhancement

Add `setLinearAcceleration(Vec3)` to LSM6DSO simulator. Add to
`generateAccelData()`. Add 2-3 tests. Existing 18 tests must still pass.

---

### W1-E: Motion Profile JSONs [Small]

**Owner:** Codex / Fusion And Kinematics
**Milestone:** M2 infrastructure

Create 12 JSON motion profiles under `simulators/motion_profiles/`.
See SIMULATION_TASKS.md (previous version) for full list and format spec.

Each must load successfully with `VirtualGimbal::loadMotionScript()`.

---

### W1-F: Python Plot Script [Medium]

**Owner:** Codex / Host Tools And Evidence
**Milestone:** M2 evidence tooling

Create `simulators/scripts/plot_test_run.py`. ~150 lines, matplotlib only.
4 standard plots: Euler tracking, angular error, raw sensor data, error
histogram. See brainstorm doc for details.

---

## Wave 2 — Depends on Wave 1

### W2-A: SimulationHarness Class [Large]

**Owner:** Codex / Fusion And Kinematics
**Milestone:** M2 core
**Depends on:** W1-A, W1-B, W1-C
**Interface spec:** `docs/simulation-harness-interface.md` (full spec)

Implement `SimulationHarness` per the interface spec. This is the critical
path item for all Wave 3 work.

---

### W2-B: Accel Calibration Tests [Medium]

**Owner:** Codex / Sensor Validation
**Milestone:** M1 validation
**Depends on:** W1-B (already done)
**Acceptance criteria:** `docs/sensor-validation-matrix.md` (LSM6DSO section)

Create `simulators/tests/test_accel_calibration.cpp`. Tests: SixPositionTumble,
NormConsistency, NoiseFloor. Own fixture (does not need SimulationHarness).

---

### W2-C: Gyro Calibration Tests [Medium]

**Owner:** Codex / Sensor Validation
**Milestone:** M1 validation
**Depends on:** W1-B (already done)
**Acceptance criteria:** `docs/sensor-validation-matrix.md` (LSM6DSO gyro section)

Create `simulators/tests/test_gyro_calibration.cpp`. Tests: StaticBiasEstimation,
KnownRateScale, IntegrationDrift. Own fixture.

---

### W2-D: Magnetometer Calibration Tests [Medium]

**Owner:** Codex / Sensor Validation
**Milestone:** M1 validation
**Depends on:** Nothing
**Acceptance criteria:** `docs/sensor-validation-matrix.md` (BMM350 section)

Create `simulators/tests/test_mag_calibration.cpp`. Tests: HardIronSphereFit,
SoftIronEllipsoidFit, TiltCompensatedHeading. Includes inline sphere-fitting
algorithm. Own fixture.

---

## Wave 3 — Depends on Wave 2

### W3-A: SimulationHarness Tests [Medium]

**Owner:** Codex / Fusion And Kinematics
**Depends on:** W2-A, W1-E

Create `simulators/tests/test_simulation_harness.cpp`. 6 tests validating
construction, stepping, determinism, config override, motion script, CSV export.

---

### W3-B: Mahony Filter Validation Tests [Medium]

**Owner:** Codex / Fusion And Kinematics
**Depends on:** W2-A
**Design reference:** `simulators/docs/CALIBRATION_SIM_BRAINSTORM.md` (section 5)

Create `simulators/tests/test_mahony_filter_validation.cpp`. Tests:
ConvergenceFromInitialError, SteadyStateDuringRotation, KiBiasRejection,
GainSweep (parameterized).

---

### W3-C: Fusion Validation Tests [Medium]

**Owner:** Codex / Fusion And Kinematics
**Depends on:** W2-A, W1-D

Create `simulators/tests/test_fusion_validation.cpp`. Tests: HighAccelNoise,
LargeGyroBias, GravityRemovalStationary, GravityRemovalWithLinearAccel,
LongDurationDrift, ParameterizedErrorSweep.

---

### W3-D: CMake Integration [Small]

**Owner:** Codex / Host Tools And Evidence
**Depends on:** All new files from W1-W3

Update `CMakeLists.txt` and `.gitignore`. Wire all new sources and test files.
Acceptance: full build + all tests pass.

---

## Wave 4 — Polish

### W4-A: CI Simulation Tier [Small]

**Owner:** Codex / Host Tools And Evidence
**Depends on:** W3-D

Add `simulation` job to `.github/workflows/ci.yml`. Runs after gate, exports
CSV, generates plots, uploads artifacts.

---

## Codex Team First Tasks

When given the go-ahead, each Codex team should start with:

| Codex Team | First Task | Why |
|------------|-----------|-----|
| Sensor Validation | W1-D (setLinearAcceleration) then W2-B (accel cal) | W1-B is already done. W1-D is small and unblocks W3-C later. W2-B is the first M1 validation task. |
| Fusion And Kinematics | W1-A (angular error metric) then W1-E (motion profiles) | W1-A is on the critical path. W1-E is independent and small. Both unblock W2-A. |
| Host Tools And Evidence | W1-C (CSV export) then W1-F (Python plots) | Both are independent and unblock nothing, so start early. |

---

## Dependency Graph

```
WAVE 1 (all parallel, start immediately)
  W1-A: Angular error metric ──────────────────┐
  W1-B: Deterministic seeding (DONE) ──────────┤
  W1-C: CSV export types ──────────────────────┤
  W1-D: setLinearAcceleration                  │
  W1-E: Motion profile JSONs                   │
  W1-F: Python plot script                     │
                                               │
WAVE 2 (partial deps)                          │
  W2-A: SimulationHarness ◄──── W1-A, W1-B, W1-C
  W2-B: Accel cal tests ◄── W1-B (done)
  W2-C: Gyro cal tests ◄── W1-B (done)
  W2-D: Mag cal tests (no deps)

WAVE 3 (harness-dependent)
  W3-A: Harness tests ◄── W2-A, W1-E
  W3-B: Mahony tests ◄── W2-A
  W3-C: Fusion tests ◄── W2-A, W1-D
  W3-D: CMake integration ◄── all files

WAVE 4
  W4-A: CI tier ◄── W3-D
```

**Critical path:** W1-A → W2-A → W3-B → W3-D
**Parallel track:** W2-B, W2-C, W2-D (sensor cal) join at W3-D

---

## Deferred Work (Milestones 3-7)

These milestones are not yet broken into wave tasks:

- **M3:** Host-side virtual node harness (MocapNodeLoop + capture transport)
- **M4:** Master-node time sync simulation
- **M5:** Network impairment model
- **M6:** Multi-node body kinematics
- **M7:** Platform port (nRF52)

These will be planned after M1 and M2 are substantially complete.
