# HelixDrift Task List

This is the execution backlog for HelixDrift.  
Rule: if a fix belongs to SensorFusion, fix it in `SensorFusion` first, push it, then update this repo's submodule pointer.

## Current Status (2026-03-07)
- [x] Repo bootstrap (`nix`, `build.py`, host/nRF build flow)
- [x] TDD blinky baseline (`BlinkEngine` + host tests)
- [x] Datasheet/manual index and fetch helper
- [x] SensorFusion submodule integrated
- [x] nRF mocap node example compiles off-target
- [x] Host tests passing (`./build.py -t`)

## In Progress
- [ ] Add CI workflow for `./build.py -t` on every push/PR

## Next Up (Priority Order)
1. [x] Add BLE sender adapter in HelixDrift example (real implementation boundary + mockable test double)
2. [x] Add mocap app-loop unit tests (timing, profile behavior, frame cadence at 50 Hz contract)
3. [x] Add calibration command flow wiring (stationary/T-pose command interface) via SensorFusion patch + submodule bump
4. [x] Add timestamp sync flow with central node contract via SensorFusion patch + submodule bump
5. [x] Add battery/health telemetry framing to mocap example path
6. [ ] Add CI workflow for `./build.py -t` on every push/PR
7. [ ] Add CI matrix smoke cross-build (host + nRF) and publish artifacts
8. [ ] Document "clone -> build -> run tests" quickstart in one page

## Done Definition (per task)
- [ ] Tests first (or test update first) and green
- [ ] Off-target build passes
- [ ] Docs updated (`README` or design page)
- [ ] If SensorFusion changed: commit/push there first, then submodule update here
