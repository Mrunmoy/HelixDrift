# Sprint 8: Milestone Redirect After Wave B Closeout

**Date:** 2026-03-29
**Author:** Claude / Systems Architect
**Branch state:** `nrf-xiao-nrf52840` at `eba1b02`, 271/271 host tests, 13 Python sidecar tests

---

## 1. Project State Assessment

### What is done

| Milestone | Status | Evidence |
|---|---|---|
| M1: Per-Sensor Proof | **~100%** | All sensor-validation-matrix gaps closed (B4). Deterministic seeding, error injection, noise stats, register behavior proven for all 3 sensors. |
| M2: Single-Node Assembly | **~95%** | Static accuracy < 1° all axes. Dynamic yaw < 10° RMS. Ki bias rejection proven. 60s drift bounded. Joint angle recovery proven. Hard-iron calibration effectiveness proven. Pitch dynamic tracking remains characterization-only. |
| M3: Node Runtime | **~50%** | VirtualMocapNodeHarness covers: cadence (50Hz), quaternion capture, anchor timestamp mapping, multi-motion regression (20 harness tests). Unit tests for MocapNodeLoop (6), health telemetry (2), timestamp sync (3). Gap: health frame capture in harness, pipeline failure handling, dropped sample detection. |

### What exists for M4+

- Kimi RF/sync simulation spec (`docs/RF_SYNC_SIMULATION_SPEC.md`) — implementation-ready
- Kimi magnetic calibration risk spec (`docs/MAGNETIC_CALIBRATION_RISK_SPEC.md`) — implementation-ready
- Kimi adversarial review — identified gaps for hardware transition
- Python sidecar pipeline — working end-to-end for experiment analysis

### Test count: 271 C++ + 13 Python = 284 total

---

## 2. Next Milestone Decision: M3 Narrow Closure

**Decision: Close M3 with a narrow sprint, then M4.**

Rationale:
- M3 is ~50% done already from the harness work. The remaining gaps are small
  and well-defined.
- M4 (RF/sync) requires the node harness to be solid because VirtualSyncNode
  wraps VirtualMocapNodeHarness.
- Closing M3 first means M4 starts on proven foundations instead of discovering
  harness gaps mid-sprint.

M3 remaining work is ~3-4 Codex tasks, not a full wave.

---

## 3. Codex Implementation Ladder

### M3 Closure Tasks (in order)

**M3-1: Health frame capture in harness [Small]**

Add health telemetry capture to VirtualMocapNodeHarness alongside the existing
quaternion capture. The harness already has `CaptureTransport` with a
`sendQuaternion()` method — extend it with `sendHealth()` (or equivalent) that
captures battery/link/calibration health frames.

Files: `simulators/fixtures/VirtualMocapNodeHarness.hpp`
Test: `VirtualMocapNodeHarnessTest.EmitsHealthFrameAtConfiguredInterval`
Acceptance: Health frame captured with expected fields. Existing 20 harness
tests still pass.

**M3-2: Pipeline failure and recovery test [Small]**

Test that `MocapNodeLoop` handles `pipeline.step()` returning false (sensor
read failure). Verify the loop does not crash, skips the frame, and resumes
on the next tick.

Files: `simulators/tests/test_virtual_mocap_node_harness.cpp`
Acceptance: Test injects a sensor failure (e.g., unregister a sensor mid-run),
verifies the loop continues and frame count is reduced.

**M3-3: Delayed/missing anchor handling test [Small]**

Test that the timestamp sync path handles delayed anchors (anchor arrives late)
and missing anchors (no anchor for an extended period). Verify frames are still
emitted with the best available timestamp mapping.

Files: `simulators/tests/test_virtual_mocap_node_harness.cpp`
Acceptance: Frames emitted even without anchor. Late anchor updates the mapping
without discontinuity.

**M3-4: Profile switching test [Small]**

Test that switching between performance mode (50Hz) and battery mode (40Hz)
changes the output cadence correctly mid-run.

Files: `simulators/tests/test_virtual_mocap_node_harness.cpp`
Acceptance: Frame interval changes from 20ms to 25ms when profile switches.

### After M3 closes

**Move to M4: RF/Sync Simulation.**

Starting point: Kimi's `docs/RF_SYNC_SIMULATION_SPEC.md`. Core tasks:
1. VirtualRFMedium (packet delivery with configurable latency/loss)
2. VirtualSyncNode (wraps harness with clock drift model)
3. VirtualSyncMaster (anchor broadcast, frame collection)
4. Basic two-way anchor sync test
5. Packet loss recovery test
6. Multi-node convergence test

This is ~24h of Codex work per Kimi's estimate.

---

## 4. Kimi Role Decision

**Kimi should switch to adversarial review of M3 closure.**

Kimi has already delivered the RF/sync and mag specs. Those are queued. There
is no new research needed right now. The highest-value Kimi contribution is
reviewing M3 closure for realism gaps — particularly:
- Does the harness health frame model match real ESP-IDF/nRF health reporting?
- Is the pipeline failure model realistic?
- Are the timestamp sync edge cases sufficient?

After M3 closes, Kimi should switch to M4 support (reviewing Codex's RF/sync
implementation against the spec Kimi wrote).

---

## 5. What Remains Deferred

| Item | Milestone | Why deferred |
|---|---|---|
| VirtualRFMedium + sync infrastructure | M4 | After M3 closes |
| MagneticEnvironment + calibration scenarios | M5-M6 | After M4 |
| Multi-node body kinematics | M6 | After M5 |
| nRF52 platform port | M7 | After M1-M3 proven |
| Pitch dynamic tracking (characterization-only) | M2 polish | Known Mahony limitation, not blocking |
| Full Kp/Ki parameterized sweep | M2 polish | Gain characterization sufficient |

---

## 6. What Should NOT Be Started

- Do NOT start M4 RF/sync implementation until M3 is closed
- Do NOT start magnetic environment infrastructure — this is M5-M6
- Do NOT start nRF52 platform bring-up — simulation proof not complete
- Do NOT re-tune Mahony Kp/Ki — current characterization is sufficient
- Do NOT expand the Python sidecar beyond the current analysis/plot pipeline
  until there are new experiment results to analyze

---

## 7. Handoff Note for Codex

**Next milestone: M3 (narrow closure, ~4 small tasks).**

Do in order:
1. Health frame capture in harness
2. Pipeline failure/recovery test
3. Delayed/missing anchor test
4. Profile switching test (50Hz ↔ 40Hz)

Each is small (~1 commit). After all 4, M3 is closed.

**Then:** Start M4 using `docs/RF_SYNC_SIMULATION_SPEC.md` as the
implementation spec. First task: VirtualRFMedium core.

**Stop conditions:**
- Do not expand M3 beyond these 4 tasks
- Do not start M4 until M3 tests pass
- Do not modify SensorFusion submodule (it is stable)
- Do not add new sensor simulator features
