# Codex Org Status

## Org Lead

- Lead session: Codex main session
- Lead worktree: repo root on `nrf-xiao-nrf52840`
- Integration worktree: repo root on `nrf-xiao-nrf52840`

## Active Teams

| Team | Worktree | Mission | Write Scope | Status |
|---|---|---|---|---|
| Sensors | repo root on `nrf-xiao-nrf52840` | Prove each sensor independently and improve simulator fidelity | `simulators/i2c/`, `simulators/sensors/`, `simulators/tests/` | active |
| Fusion | repo root on `nrf-xiao-nrf52840` | Build host-side virtual sensor and node harnesses on top of the proven simulator stack | `simulators/fixtures/`, `simulators/tests/`, `tests/` | active |
| Host Tools | repo root on `nrf-xiao-nrf52840` | Extend deterministic evidence capture around the proven simulator stack | `simulators/fixtures/`, `simulators/tests/`, `docs/`, `tools/analysis/` | active |
| nRF52 |  | Defer until simulation proof work advances | `examples/nrf52-mocap-node/`, `tools/nrf/` | idle |

## Planning Gate

- Problems currently owned:
  - Per-sensor validation matrix and standalone proof criteria
  - Deterministic simulator behavior needed for quantitative sensor tests
  - Register-behavior and scale-behavior proof items that are already
    implemented in the simulators
  - Host-side virtual sensor-assembly harness for reusable dual-bus,
    three-sensor composition
  - Host-side virtual mocap node harness with cadence, capture transport, and
    timestamp-mapping proof
  - Basic pose-quality metrics suitable for bounded host assertions
  - Scripted yaw motion-regression assertions on the virtual node harness
  - Batched run support with summary pose-error stats for future experiments
  - Harness safety and deterministic-seeding coverage in the Codex worktree
  - Wave A A5 Mahony bias-rejection proof in a standalone experiment file
  - Wave A A1 static-yaw probe and escalation evidence
  - Wave A A3 long-duration drift proof for identity-start operation
  - Wave A A6 two-node joint-angle proof for near-identity initialization
  - Additional A1a and A2 probe evidence showing current redirected thresholds
    are still too aggressive for the present SensorFusion behavior
  - SensorFusion first-sample Mahony seeding fix committed locally in the
    submodule to replace identity-only startup
  - A4 yaw-gain characterization showing that higher `MahonyKp` currently
    worsens both small static yaw offsets and 30 deg/s dynamic yaw tracking
  - Axis-split characterization showing that yaw remains materially better than
    pitch/roll for both small static offsets and 30 deg/s dynamic tracking
  - Yaw-only `A1a` and yaw-only `A2` acceptance slices committed after Claude's
    Sprint 6 rescope
  - SensorFusion init convention bug documented for pitch/roll startup
  - Wave B `B1` CSV export foundation for deterministic host evidence capture
  - Wave B `B1` sidecar-compatible manifest plus samples export for Python
    analysis handoff
  - Wave B `B1` Python analysis and plotting CLIs validated against exported
    host-test artifacts
  - Wave B `B2` motion-profile JSON catalog with loader coverage
  - Wave B `B4` first validation-gap closure batch for LPS extremes and LSM
    baseline physical/noise checks
  - Wave B `B4` BMM orientation/error batch for pitch projection, hard-iron
    constancy, and noise statistics
  - Wave B `B3` hard-iron calibration effectiveness proof using the existing
    SensorFusion fitter and a test-local calibrated magnetometer wrapper
  - Wave B `B4` closure with no remaining standalone matrix gaps
  - M3 health-frame capture through the virtual node harness using the real
    FrameCodec path
  - M3 pipeline failure/recovery proof by disconnecting and reconnecting the
    IMU device mid-run
  - M3 delayed-anchor proof showing local timestamps before sync and remapped
    timestamps after a late anchor arrives
  - M3 cadence-switch proof covering a mid-run change from 50 Hz to 40 Hz
  - M4 `VirtualRFMedium` core with deterministic latency, broadcast, and loss
    coverage
  - M4 `ClockModel`, `VirtualSyncNode`, and `VirtualSyncMaster` basic sync
    loop with drift, anchor reception, and six-node convergence coverage
  - M4 RF robustness coverage for 50% Bernoulli loss, continued frame
    transmission during degraded sync, and recovery after a 2-second blackout
  - M5 `MagneticEnvironment` core with Earth-field modeling, dipole-source
    superposition, preset disturbance environments, and standalone host tests
  - M5 additive `Bmm350Simulator` environment hook with coverage for
    environment-driven field override and position-sensitive dipole influence
  - M5 standalone `HardIronCalibrator` with deterministic offset estimation,
    confidence gating, and reset behavior coverage
  - M5 reusable `CalibratedMagSensor` wrapper proven in both unit tests and
    the pose-calibration effectiveness integration test
  - M5 disturbance-aware `CalibratedMagSensor` hook with environment-derived
    disturbance indicator coverage
  - M5 AHRS robustness proof covering heading degradation during a temporary
    magnetic disturbance and bounded recovery after the disturbance is removed
- Writable scopes currently claimed:
  - `simulators/sensors/`
  - `simulators/fixtures/`
  - `simulators/tests/`
  - `docs/`
  - `simulators/docs/DEV_JOURNAL.md`
- Review-only scopes:
  - `.agents/`
  - `TASKS.md`
  - RF/sync design topics owned by Kimi
  - Pose requirements and systems planning owned by Claude
- Blocked or contested scopes:
  - `firmware/common/` unless needed by a later assigned Codex task
  - any scope actively claimed later by another org
- No-duplication check completed:
  - Codex owns sensor implementation and sensor tests
  - Codex does not own primary RF/sync research
  - Codex does not own primary pose-inference requirements work
- Approved to execute:
  - Yes, for the Sensor Validation lane only

## Claimed Scopes

- Active implementation ownership:
  - `simulators/sensors/`
  - `simulators/tests/`
- Review-only areas:
  - `.agents/`
  - most architecture docs under `docs/`
- Conflicts or blocked scopes:
  - none active at this time

## Current Work

- Task: Add deterministic per-sensor simulator seeding and document standalone sensor proof criteria
- Task: Deliver the Sensor Validation slice, reusable sensor-assembly harness,
  virtual mocap node harness, and first pose-metric assertions for host
  integration work
- Task: Close M2 Wave A incrementally from the Codex worktree, starting with
  harness-safety coverage and scripted yaw regressions before broader pose
  experiments
- Task: Follow Claude Wave A sequencing, but escalate any task that fails its
  intermediate-entry conditions instead of forcing false acceptance tests
- Task: After Claude Sprint 5 redirect, close A3 first, then move to A1a
  small-offset static accuracy instead of retrying blocked large-angle cases
- Task: Keep A1a and A2 in evidence-gathering mode until Claude or SensorFusion
  changes justify codifying them as acceptance tests
- Task: Use A6 joint-angle recovery as the next valid M2 proof slice because it
  remains accurate despite the blocked absolute static-offset path
- Task: Carry the local SensorFusion seeding commit through Helix as a
  submodule-pointer update because it meaningfully improves static yaw startup
  while leaving pitch/roll and dynamic-tracking limits still open
- Task: Use the new yaw-gain characterization as the current A4 result instead
  of assuming higher `Kp` will rescue A1a/A2 yaw behavior
- Task: Treat A1a and A2 as axis-split characterization problems until Claude
  explicitly decides whether yaw-only acceptance is a legitimate intermediate
  milestone
- Task: Carry the SensorFusion AHRS convention fix through the submodule and
  rebaseline Helix characterization tests against the fixed behavior instead of
  preserving tests for already-fixed failure modes
- Task: Close M3 with the four bounded runtime-harness slices from Claude
  Sprint 8: health capture, failure/recovery, late anchors, and cadence
  switching
- Task: Start Wave B with the C++ half of `B1`: deterministic CSV export from
  `NodeRunResult`, with Python analysis left to the sidecar lane
- Task: Close `B2` with a checked-in motion-profile catalog and a host test
  that loads every profile via `VirtualGimbal`
- Task: Continue `B4` by closing standalone sensor-matrix gaps in small
  batches, starting with tests that require no new simulator features
- Task: Keep `B1` moving until the C++ export lane is ready for clean handoff
  into the Python sidecar
- Task: Treat Wave B evidence capture as closed for now; the next mainline move
  should come from a fresh milestone redirect rather than more evidence churn
- Task: Start M4 with the additive RF slice only: `VirtualRFMedium` core and
  its basic latency, broadcast, and loss proofs
- Task: Extend M4 from RF transport into the first sync loop: drifted
  `VirtualSyncNode`, anchor-broadcast `VirtualSyncMaster`, and basic
  single-node / loss / multi-node convergence tests
- Task: Harden M4 with transport-blackout recovery and sustained degraded-sync
  behavior before moving to magnetic disturbance work
- Task: Start M5 with an additive `MagneticEnvironment` core before touching
  BMM350 integration or calibration-state logic
- Task: Keep M5 additive while wiring `MagneticEnvironment` into the existing
  BMM350 simulator before introducing calibration state or AHRS robustness work
- Task: Land the first standalone hard-iron calibration primitive before any
  calibrated-sensor wrapper or body-chain scenarios
- Task: Replace test-local magnetic calibration plumbing with reusable
  simulator code before expanding into richer disturbance and multi-node cases
- Task: Prove the first end-to-end magnetic disturbance and recovery path
  through the real `Bmm350Simulator` and `MocapNodePipeline` before opening M6
  body-chain work
- Task: Start M6 with bounded three-node/body-chain scenarios only after the
  disturbance path is host-verified
- Design doc: `docs/PER_SENSOR_VALIDATION_MATRIX.md`
- Tests first: yes
- Journal updated: yes (`simulators/docs/DEV_JOURNAL.md`)

## Reviews

- Requested from:
- Received from:
- Findings outstanding:

## Integration State

- Ready to merge into codex integration: yes
- Waiting on fixes: no
- Ready for top-level integration: yes
