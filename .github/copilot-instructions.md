# Copilot Instructions for HelixDrift

## Project Context

HelixDrift is a simulation-first mocap node firmware workspace targeting **nRF52840**. Host simulation is the source of truth for fusion, sync, calibration, and multi-node behavior; the nRF path stays thin and follows already-proven contracts.

This branch (`nrf-xiao-nrf52840`) is **nRF-only**. Do not add ESP32 or other secondary-MCU-specific code or docs here.

Target sensor stack: LSM6DSO (IMU), BMM350 (magnetometer), LPS22DF (barometer).

## Build & Test Commands

All commands run from the repo root. The build system uses `nix develop` under the hood.

```bash
# Host build + all tests (primary dev loop)
./build.py --host-only -t

# nRF cross-compile only (when platform code changes)
./build.py --nrf-only

# Clean build
./build.py --clean

# Convenience wrapper (host tests + nRF build)
./magic.sh

# Run a single unit test by name
./build/host/helix_tests --gtest_filter='BlinkEngineTest.StartsOnByDefault'

# Run a single integration test by name
./build/host/helix_integration_tests --gtest_filter='VirtualSensorAssemblyTest.*'

# Run tests matching a CTest pattern
ctest --test-dir build/host --output-on-failure -R test_blink_engine
```

## Architecture

Two layers of code, two namespaces:

- **`helix`** namespace — HelixDrift application code in `firmware/common/` (BlinkEngine, MocapNodeLoop, MocapBleSender, OTA manager/service, timestamp sync transport)
- **`sf`** namespace — SensorFusion library in `external/SensorFusion/` (sensor drivers, AHRS, codec, motion pipeline, calibration). This is a git submodule.

Key structural areas:

| Area | Location | Purpose |
|------|----------|---------|
| Common firmware | `firmware/common/` | Platform-agnostic embedded-safe runtime (no heap/RTTI/exceptions) |
| OTA subsystem | `firmware/common/ota/` | MCUboot trailer, UART OTA protocol, OTA manager, BLE OTA service |
| Unit tests | `tests/` | GoogleTest tests for `helix` code; binary: `helix_tests` |
| Simulators | `simulators/` | Virtual I2C, sensor simulators, gimbal, magnetic environment, RF medium |
| Integration tests | `simulators/tests/` | GoogleTest tests using simulators; binary: `helix_integration_tests` |
| nRF examples | `examples/nrf52-*/` | Board-specific main apps (mocap node, blinky, OTA, bringup, selftest) |
| SensorFusion | `external/SensorFusion/` | Drivers (LSM6DSO, BMM350, LPS22DF, etc.), MahonyAHRS, FrameCodec, MocapNodePipeline |

### SensorFusion Submodule Rule

If a fix belongs to SensorFusion, commit and push it in `external/SensorFusion/` first, then update the submodule pointer in this repo.

### SensorFusion Internals (for `sf` namespace work)

HAL interfaces in `drivers/hal/`: `II2CBus`, `ISPIBus`, `IDelayProvider`, `INvStore`, `IGpioInterrupt`, `IGpioInput`, `IGpioOutput`, `IAdcChannel`.

Sensor drivers take HAL references via constructor injection. Middleware includes MahonyAHRS (9-DOF complementary filter → quaternion), FrameCodec (CRC-16 CCITT), MocapNodePipeline (linear accel, forward kinematics, ZUPT).

## Coding Conventions

**Language**: C++17. No exceptions — use `bool` return values for error propagation.

**Namespaces**: `helix` for this repo, `sf` for SensorFusion.

**Naming**:
- Classes/types: `PascalCase` (`BlinkEngine`, `MahonyAHRS`)
- Methods/functions: `camelCase` (`readAccel`, `sendQuaternion`)
- Private members: trailing underscore (`bus_`, `cfg_`, `nextTickUs_`)
- Constants / register addresses: `UPPER_SNAKE_CASE`
- Enum classes: `PascalCase` variants (`enum class AccelRange { G2, G4, G8, G16 }`)

**File layout**: one class per file (`ClassName.hpp` / `ClassName.cpp`), `#pragma once`, register constants in unnamed namespace at top of `.cpp`.

**Error handling**: guard-clause style — return `false` immediately on errors; no deep nesting.

**Formatting**: 4-space indentation, braces on same line as control statements.

**Templates**: `MocapNodeLoopT` pattern — template on Clock/Pipeline/Transport/Sample to enable host testing with mocks while compiling the same logic for nRF.

## Testing

Two test binaries, both GoogleTest + GMock:

- **`helix_tests`** — unit tests in `tests/test_*.cpp`, mocks in `tests/mocks/`
- **`helix_integration_tests`** — simulator-backed tests in `simulators/tests/test_*.cpp`

Test coverage expectations for new code: init success, init failure (bus/HW error), data correctness, and math/conversion accuracy.

## CI

GitHub Actions workflow: `.github/workflows/ci.yml`
- Host unit job: `helix_tests`
- Host integration job: `helix_integration_tests`

## Commit Style

Imperative, scoped summaries. Include test evidence (command + pass count) in PRs.

```
Add portable LPS22DF barometric pressure driver with 11 tests
Fix nRF52840 GPIO register layout for bare-metal LED drive
```

## Workflow Summary

1. Write or update tests first where practical.
2. Implement in `firmware/common/`, `simulators/`, or the nRF example as appropriate.
3. Run `./build.py --host-only -t`.
4. If platform code changed, run `./build.py --nrf-only`.
5. Update docs and task tracking before handoff.
