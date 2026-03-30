# HelixDrift Task List

This is the execution backlog for HelixDrift.
Rule: if a fix belongs to SensorFusion, fix it in `SensorFusion` first, push it, then update this repo's submodule pointer.

## Current Status (2026-03-29)
- [x] Repo bootstrap (`nix`, `build.py`, host/nRF build flow)
- [x] TDD blinky baseline (`BlinkEngine` + host tests)
- [x] Datasheet/manual index and fetch helper
- [x] SensorFusion submodule integrated
- [x] nRF mocap node example compiles off-target
- [x] Host and simulator tests passing (`./build.py --clean --host-only -t`)
- [x] Single-node simulator integration green (218+ host tests passing)
- [x] Per-sensor validation matrix defined
- [x] Virtual sensor-assembly harness delivered
- [x] Virtual mocap node harness with configurable Kp/Ki, setSeed, RunResult
- [x] SimMetrics angular error utility delivered
- [x] Pose inference v1 requirements defined (orientation-only)
- [x] Pose inference feasibility analysis complete

## Mission Focus

HelixDrift is simulation-first until real hardware exists.

The primary goal is to prove that a small wearable sensor node built from
IMU + magnetometer + barometer can:

- estimate orientation robustly from known motion inputs
- maintain useful timing alignment with a master node and peer nodes
- support low-latency mocap streaming before platform-specific hardware exists

MCU targets are implementation platforms, not the core product goal. The
current intended primary target is nRF52.

## Active Focus: M5 Magnetic Robustness And M6 Multi-Node Realism

See `docs/CODEX_NEXT_WAVES.md` for detailed execution plan and acceptance criteria.

### Wave A — Immediate (Codex / Fusion)

Wave A was re-scoped during Sprint 6/7 after the SensorFusion initialization
fix. The original table below is now interpreted through the Claude redirect
notes in `docs/SPRINT6_WAVE_A_RESCOPE.md` and
`docs/SPRINT7_POST_FIX_ASSESSMENT.md`.

| # | Task | Acceptance | Status |
|---|------|-----------|--------|
| A1 | Static multi-pose orientation accuracy (7 poses) | Static all-axis accuracy proved after SensorFusion convention fix | done |
| A2 | Dynamic single-axis tracking (yaw/pitch/roll 30°/s) | Yaw + roll accepted; pitch retained as characterization-only | partial |
| A3 | 60s heading drift (Kp=1.0, Ki=0.02, clean) | Max error < 10°, drift < 2°/min | done |
| A4 | Mahony Kp/Ki convergence sweep (12 combos) | Gain behavior characterized and redirected by Claude | done |
| A5 | Gyro bias rejection via Ki (0.01 rad/s bias) | Ki=0: drift > 5°/min; Ki=0.05: < 2°/min | done |
| A6 | Two-node joint angle recovery (5 poses) | Error < 5° all poses, < 7° max | done |

### Wave B — After Wave A

| # | Task | Owner | Status |
|---|------|-------|--------|
| B1 | CSV export + Python plot script | Codex / Host Tools | done |
| B2 | Motion profile JSON library (12 files) | Codex / Fusion | done |
| B3 | Hard iron calibration effectiveness test | Codex / Sensor Validation | done |
| B4 | Sensor validation matrix remaining gaps | Codex / Sensor Validation | done |

### Deferred — M4+ (after M2 closes)

| Task | Milestone | Blocked By |
|------|-----------|-----------|
| VirtualRFMedium + VirtualSyncNode/Master | M4 | RF/sync spec review |
| Network impairment tests | M4 | VirtualRFMedium |
| Multi-node sync convergence | M4 | VirtualSyncNode |
| MagneticEnvironment class | M5-M6 | in progress |
| CalibratedMagSensor + disturbance scenarios | M5-M6 | in progress |
| Three-node body-chain scenarios | M6 | M4 + M5 |
| Wire validated runtime into nRF52 | M7 | M1-M3 complete |

## Milestone Summary

| Milestone | % | Current Focus |
|-----------|---|---------------|
| M1: Per-Sensor Proof | ~100% | Wave B4 closed the remaining standalone gaps |
| M2: Single-Node Assembly | ~90% | Wave A evidence is merged; pitch dynamic remains characterized |
| M3: Node Runtime | ~100% | Harness runtime closure landed: health, recovery, anchors, cadence switching |
| M4: RF/Sync | ~80% | Basic sync, convergence, and blackout recovery are landed; transport core is ready for later hardware sanity checks |
| M5-M6: Calibration + Multi-node | ~50% | MagneticEnvironment, BMM350 environment hookup, hard-iron calibration, reusable calibrated-mag path, and first AHRS disturbance-recovery proof are landed |
| M7: Platform Port (nRF52) | 0% | Deferred |

## Reference Documents

- Execution plan: `docs/CODEX_NEXT_WAVES.md`
- Simulation backlog: `docs/SIMULATION_BACKLOG.md`
- Sensor validation criteria: `docs/sensor-validation-matrix.md`
- Harness interface spec: `docs/simulation-harness-interface.md`
- Pose requirements: `docs/pose-inference-requirements.md`
- RF/sync spec: `docs/RF_SYNC_SIMULATION_SPEC.md`
- Mag risk spec: `docs/MAGNETIC_CALIBRATION_RISK_SPEC.md`
- Claude review reports: `docs/CLAUDE_ORG_SPRINT2_REPORT.md`

## Done Definition (per task)
- [ ] Tests first (or test update first) and green
- [ ] Off-target build passes
- [ ] Docs updated (`README` or design page)
- [ ] If SensorFusion changed: commit/push there first, then submodule update here
