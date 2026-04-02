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
  - M5 AHRS disturbance characterization proving a temporary horizontal
    magnetic disturbance can poison heading persistently after the disturbance
    is removed
  - M6 three-node static body-chain proof with recovered 45°/45° joint angles
    and bounded inter-node sync skew at the RF master
  - M6 three-node dynamic hinge proof with post-warmup bounded mean joint-angle
    error and bounded inter-node skew across a moving elbow/wrist chain
  - M6 long-run mild-impairment characterization proving the chain can remain
    synchronized while relative-angle accuracy drifts badly over sustained
    jitter/loss
  - nRF-branch cleanup removing legacy ESP32-S3 example, OTA backend, host
    stubs, and ESP-IDF-oriented build/docs references from
    `nrf-xiao-nrf52840`
  - M7 real-hardware bring-up on an available Nordic nRF52 DK, including
    proven SWD flashing through the onboard SEGGER J-Link probe
  - dedicated `nrf52dk_blinky` and `nrf52dk_bringup` targets for the connected
    nRF52832 DK, separated from the intended nRF52840 product path
  - OpenOCD-based flashing helper and automatic `.bin` / `.hex` artifact
    generation for nRF targets
  - webcam-assisted verification that the flashed DK heartbeat target is
    producing a periodic LED transition consistent with the programmed cadence
  - standalone bare-metal `nrf52dk_selftest` path with direct GPIO/delay
    headers, DK-specific linker/startup path, retained `.noinit` status block,
    and proven internal flash erase/write/verify on the real nRF52832 DK
  - real-hardware OTA-backend validation on the DK, including configurable
    flash-region targeting, correct partial-word merge behavior, tail-partial
    writes, and bounds rejection proven through `nrf52dk_selftest`
  - real-hardware OTA manager/service proof on the DK through synthetic
    begin/data/commit traffic routed via `BleOtaService`, `OtaManager`, and
    `NrfOtaFlashBackend` into a dedicated test flash slot
  - real-hardware MCUboot promotion proof on the DK using staged
    `nrf52dk_ota_probe_v1` / `nrf52dk_ota_probe_v2` images, explicit
    overwrite-only pending markers, and repo-local OpenOCD smoke helpers
  - nix-only nRF developer contract hardened so repo clone + submodules +
    `nix develop` now covers doctor/build/sign/bootloader paths without
    relying on manual NCS / Zephyr / imgtool installs
  - standalone MCUboot build ported onto the vendored top-level `third_party/mcuboot`
    tree with repo-local crypto, key, and flash-map glue for the nRF branch
  - measured standalone bootloader size proving the old 64 KB plan was too
    small; branch layout updated to a 96 KB bootloader slot with 352 KB
    primary/secondary slots and a 192 KB NVS region
  - DK virtual COM path now proven on real hardware using Nordic's documented
    `UART0 TX=P0.06` / `RX=P0.08` routing, with live output confirmed on
    `/dev/ttyACM0`
  - repo-local nRF52 bare-metal GPIO helper corrected to Nordic's real
    register layout, restoring the DK LED heartbeat path on `P0.17`
  - repo-local Zephyr/NCS Helix BLE OTA app for `nrf52dk/nrf52832`, built from
    the nix shell through a bootstrapped `.deps/ncs/` workspace
  - real Helix BLE OTA proven end to end on the connected DK:
    `HelixOTA-v1` accepts a signed `v2` image over BLE from this PC, stages
    it, commits it, reboots through MCUboot, and comes back advertising
    `HelixOTA-v2`
- Writable scopes currently claimed:
  - `simulators/sensors/`
  - `simulators/fixtures/`
  - `simulators/tests/`
  - `docs/`
  - `simulators/docs/DEV_JOURNAL.md`
  - `tools/nrf/`
  - `zephyr_apps/`
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
- Task: Close the remaining M7 OTA gap by binding the proven Helix OTA manager
  and backend path to a real BLE transport on the connected DK and verifying a
  real `v1 -> v2` promotion over the air
- Task: Start M6 with bounded three-node/body-chain scenarios only after the
  disturbance path is host-verified
- Task: Expand M6 from the first static three-node chain proof into dynamic
  multi-node/body-chain scenarios without reopening RF or SensorFusion work
- Task: Push M6 from bounded three-node proofs toward longer-running or higher-
  node chain scenarios on top of the current RF + magnetic stack
- Task: Close M5-M6 on the simulation side with honest documentation of the
  long-run impaired-chain limitation, then hand off to M7 hardware prep
- Task: Strip legacy ESP32-specific code paths and documentation from the nRF
  branch before M7 bring-up so nRF platform work starts from a single-target
  branch
- Task: Use the available nRF52 DK as a generic hardware bring-up target
  without conflating it with the final nRF52840 board assumptions
- Task: Keep the nix-only developer contract intact while moving the next
  hardware-observability step from retained-RAM readback toward live DK VCOM
  output on the corrected Nordic UART pins
- Task: Prove serial/VCOM output or otherwise establish a reliable real-board
  runtime observability path on the DK
- Task: Treat retained-RAM status plus direct flash readback as the current
  authoritative DK observability path while serial/VCOM remains open
- Task: Use the DK self-test path to close board-local flash/backend questions
  before attempting full OTA transport on hardware
- Task: Treat OTA state handling as proven on the DK; remaining OTA work is now
  the real BLE transport path into the already-proven backend + MCUboot chain
- Task: Push M7 from proven SWD flashing into real OTA-path validation once
  the DK runtime observability path is credible
- Task: Treat UART/VCOM OTA as the current stable remote-update path on the DK
  and use it to harden OTA semantics while BLE transport remains unimplemented
  in-repo
- Task: Remove dependence on a personal Nordic toolchain install by making the
  BLE reference lane reproducible from `nix develop` plus repo-local scripts
- Task: Keep the Nordic/Zephyr BLE lane explicitly reference-only until a real
  HelixDrift target app advertises OTA characteristics on hardware
- Task: Use the recovered Nordic UART Service smoke path as the current proof
  that this PC, BLE adapter, DK, and nix shell can exercise a real over-the-
  air data path before replacing the reference sample with a HelixDrift BLE
  target
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

## Latest Milestone Note

- M7 UART OTA is now proven end-to-end on real hardware:
  - signed `nrf52dk_ota_serial_v1` boots through MCUboot
  - repo-local uploader stages signed `nrf52dk_ota_serial_v2` over
    `/dev/ttyACM0`
  - the app commits via the real OTA backend
  - MCUboot promotes the image
  - the board comes back reporting `ota-v2`
- Remaining M7 OTA gap:
  - real BLE transport path
