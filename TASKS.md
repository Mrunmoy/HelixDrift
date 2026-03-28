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
- [x] Single-node simulator integration green (`201/201` host tests passing)

## Mission Focus

HelixDrift is simulation-first until real hardware exists.

The primary goal is to prove that a small wearable sensor node built from
IMU + magnetometer + barometer can:

- estimate orientation robustly from known motion inputs
- maintain useful timing alignment with a master node and peer nodes
- support low-latency mocap streaming before platform-specific hardware exists

MCU targets are implementation platforms, not the core product goal. The
current intended primary target is nRF52.

## In Progress
- [ ] Document "clone -> build -> run tests" quickstart in one page
- [ ] Rebase backlog and docs around simulation-first system proof

## Next Up (Priority Order)
1. [x] Add explicit per-sensor validation matrix for IMU, magnetometer, and barometer before fusion-level testing
2. [ ] Expand single-sensor simulator tests to cover scale, bias, noise, orientation response, and driver init/probe behavior with quantitative thresholds
3. [x] Add host-side virtual sensor-assembly harness that combines the three proven sensors into one node-level test fixture
4. [ ] Add host-side virtual mocap node harness combining simulator sensors, SensorFusion pipeline, `MocapNodeLoop`, timestamp mapping, and a fake transport
5. [ ] Add scripted motion regression suite for known pose, constant-rate rotation, oscillation, compound rotation, and return-to-origin scenarios
6. [ ] Define orientation quality metrics and assert them in tests (max angular error, RMS error, drift after N seconds)
7. [ ] Add master-node timebase simulator and anchor-flow tests for multi-node sync
8. [ ] Add network impairment simulator for latency, jitter, packet loss, and packet reordering
9. [ ] Add two-node kinematic validation scenarios for relative joint-angle recovery
10. [ ] Add three-node body-chain scenarios to validate master-aligned time fusion
11. [ ] Wire the validated common runtime into the nRF52 example path
12. [ ] Keep ESP32-S3 support optional and secondary unless it provides clear value
13. [ ] Document "clone -> build -> run tests" quickstart in one page

## Detailed Backlog

See `docs/SIMULATION_BACKLOG.md` for milestone sequencing, acceptance criteria,
and file-level implementation targets.

## Done Definition (per task)
- [ ] Tests first (or test update first) and green
- [ ] Off-target build passes
- [ ] Docs updated (`README` or design page)
- [ ] If SensorFusion changed: commit/push there first, then submodule update here
