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
- [x] nRF branch scrubbed of legacy ESP32/ESP-IDF paths and docs
- [x] nix-only nRF developer shell hardened with vendored MCUboot and repo-local doctor/sign/flash helpers

## Mission Focus

HelixDrift is simulation-first until real hardware exists.

The primary goal is to prove that a small wearable sensor node built from
IMU + magnetometer + barometer can:

- estimate orientation robustly from known motion inputs
- maintain useful timing alignment with a master node and peer nodes
- support low-latency mocap streaming before platform-specific hardware exists

MCU targets are implementation platforms, not the core product goal. The
current intended primary target is nRF52.

The `nrf-xiao-nrf52840` branch is now maintained as nRF-only. Legacy ESP32-S3
example code, host stubs, OTA backend wiring, and ESP-IDF-oriented docs were
removed so platform-specific work stays isolated to the ESP branch.

## Active Focus: M7 Hardware Bring-Up Preparation

See `docs/CODEX_NEXT_WAVES.md` for detailed execution plan and acceptance criteria.

Current M7 bring-up progress:
- [x] clone + submodules + `nix develop` is now sufficient for HelixDrift host build,
      nRF build, standalone bootloader build, and signed-image generation
- [x] `nix develop` now provides OpenOCD, pyOCD, west, and serial tools
- [x] nRF builds now emit `.bin` and `.hex` artifacts automatically
- [x] OpenOCD flashing is proven on a connected Nordic nRF52 DK
- [x] dedicated `nrf52dk_blinky` and `nrf52dk_bringup` targets exist for the
      available nRF52832 DK hardware
- [x] webcam-assisted observation shows a periodic LED transition on the DK
      consistent with the flashed heartbeat target
- [x] DK bring-up LED heartbeat is electrically proven after fixing the
      repo-local nRF52 GPIO register layout used by the bare-metal helpers
- [x] dedicated `nrf52dk_selftest` proves a real bare-metal boot path on the
      DK, direct LED drive, and internal flash erase/write/verify on target
- [x] serial/VCOM output from the custom DK bring-up app is proven on
      `/dev/ttyACM0` with Nordic's documented `P0.06/P0.08` VCOM routing
- [x] OTA-backend-specific chunk, tail-partial, and bounds behavior is now
      proven on real hardware via the DK self-test
- [x] OTA manager/service state-machine path is now proven on real hardware via
      synthetic begin/data/commit self-test traffic on the DK
- [x] standalone MCUboot boot path on the DK now performs a real
      `v1 -> staged v2` slot-1 upgrade into the primary slot and boots the
      updated image successfully
- [x] UART/VCOM OTA transport is now proven end-to-end on the DK:
      signed `ota-v1` accepts a signed `ota-v2` image over `/dev/ttyACM0`,
      stages it through the real backend, commits, reboots via MCUboot, and
      comes back reporting `ota-v2`
- [x] standalone MCUboot build now fits the branch layout after resizing the
      bootloader slot to `96 KB` and the primary/secondary slots to `352 KB`
- [x] `nix develop` now includes the extra Zephyr/Nordic host utilities needed
      for a BLE reference lane (`dtc`, `gperf`, `bluez`, `pkg-config`,
      `pyserial`, `bleak`)
- [x] repo-local scripts now bootstrap a pinned Nordic Connect SDK workspace
      under `.deps/ncs/` and build a BLE reference sample without requiring a
      separately installed personal NCS toolchain
- [x] repo-local BLE reference smoke now recovers the DK, flashes a Nordic
      UART sample, discovers it over BLE from this PC, and verifies payload
      delivery on the DK UART console
- [x] HelixDrift-target BLE OTA transport is now proven end to end on the DK:
      a repo-local Zephyr/NCS app exposes the Helix OTA GATT service,
      this PC uploads a signed `v2` image over BLE, the board stages it,
      reboots through MCUboot, and comes back advertising `HelixOTA-v2`
- [x] OTA target identity is now enforced on the DK path:
      uploaders send an explicit target ID, the running image reports its
      own target ID, and a 52840-targeted OTA is rejected immediately by the
      52832 DK before any transfer begins
- [x] BLE OTA failure handling is now proven on the DK:
      bad CRC is rejected without promoting `v2`, explicit abort returns the
      device to idle on `v1`, and a later good BLE update still succeeds
- [x] a dedicated `nrf52840dongle_blinky` target now exists for the soldered
      nRF52840 dongle path, with an external-J-Link-selectable flashing flow
- [x] repo-native `nrf52840dongle_blinky` now boots on the dongle after
      aligning the 52840 bare-metal startup with the DK's minimal-init path
- [x] SWD samples on the dongle prove the core stays in `Thread` mode and
      `GPIO0.OUT` toggles bit 6 as expected for `LED0`
- [ ] `nrf52840` BLE OTA is narrowed but not closed:
      `Helix840-v1` now boots and advertises on the real dongle, `BEGIN`
      succeeds once the uploader allows enough time for the full-slot erase,
      and the first dongle can stream most or all of the signed `v2` image;
      the remaining blocker is a clean, repeatable `slot1 -> pending ->
      reboot into Helix840-v2` closure on `nrf52840dongle/nrf52840/bare`
- [ ] stale 52840 OTA helper assumptions still need one cleanup pass:
      the manual pending-trailer helper was based on an old hand-written
      28-byte footer and must be aligned to the build-generated
      `CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE=0x30` contract before it is useful as
      a validation tool again
- [ ] once the first `nrf52840` dongle proves `Helix840-v1 -> Helix840-v2`
      over BLE cleanly, repeat the same flow on the second 52840 target and
      then lock the 52840 BLE OTA lane with updated README/how-to docs

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
| MagneticEnvironment class | M5-M6 | done |
| CalibratedMagSensor + disturbance scenarios | M5-M6 | done |
| Three-node body-chain scenarios | M6 | done |
| Wire validated runtime into nRF52 | M7 | M1-M3 complete |

## Milestone Summary

| Milestone | % | Current Focus |
|-----------|---|---------------|
| M1: Per-Sensor Proof | ~100% | Wave B4 closed the remaining standalone gaps |
| M2: Single-Node Assembly | ~90% | Wave A evidence is merged; pitch dynamic remains characterized |
| M3: Node Runtime | ~100% | Harness runtime closure landed: health, recovery, anchors, cadence switching |
| M4: RF/Sync | ~80% | Basic sync, convergence, and blackout recovery are landed; transport core is ready for later hardware sanity checks |
| M5-M6: Calibration + Multi-node | ~100% | Simulation-side calibration, disturbance characterization, and three-node body-chain proofs are landed; long-run mild-impairment drift is documented as a current limitation |
| M7: Platform Port (nRF52) | ~99% | DK flashing, bare-metal boot, LED drive, serial/VCOM, raw NVMC, OTA backend, UART OTA transport, MCUboot slot promotion, repo-local BLE reference workflow, real HelixDrift BLE OTA, wrong-target OTA rejection, BLE OTA failure-path handling on the DK, and first repo-native nRF52840 dongle bring-up are proven; attached-sensor bring-up and multi-node RF hardware work remain open, and the remaining 52840-specific OTA closure is narrowed to the dongle pending/commit boot path |

## Reference Documents

- Execution plan: `docs/CODEX_NEXT_WAVES.md`
- Simulation backlog: `docs/SIMULATION_BACKLOG.md`
- Sensor validation criteria: `docs/sensor-validation-matrix.md`
- Harness interface spec: `docs/simulation-harness-interface.md`
- Pose requirements: `docs/pose-inference-requirements.md`
- RF/sync spec: `docs/RF_SYNC_SIMULATION_SPEC.md`
- Mag risk spec: `docs/MAGNETIC_CALIBRATION_RISK_SPEC.md`
- Claude review reports: `docs/CLAUDE_ORG_SPRINT2_REPORT.md`
- BLE reference lane: `docs/NRF_BLE_REFERENCE_WORKFLOW.md`

## Done Definition (per task)
- [ ] Tests first (or test update first) and green
- [ ] Off-target build passes
- [ ] Docs updated (`README` or design page)
- [ ] If SensorFusion changed: commit/push there first, then submodule update here
