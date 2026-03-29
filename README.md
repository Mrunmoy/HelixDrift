# HelixDrift

Simulation-first mocap node firmware workspace.

> Intended primary MCU target: nRF52
> Current automated validation target: host simulation and host tests

## Quick Start

```bash
./build.py --clean --host-only -t
```

This runs the full host-side build and test suite, including simulator-backed
integration tests.

## Build Commands

- `./build.py --host-only -t`: host build and tests only.
- `./build.py --esp32s3-only`: optional legacy ESP32-S3 build path.
- `./build.py --clean`: clean then build.
- `./magic.sh`: legacy convenience wrapper for host + ESP32-S3 build.

`build.py` auto-initializes submodules and calls platform tooling when needed.

## CI

GitHub Actions workflow: `.github/workflows/ci.yml`

- Host unit job: builds host targets and runs `./build/host/helix_tests`
- Host integration job: builds host targets and runs `./build/host/helix_integration_tests`
- There are no automated platform runtime or platform smoke jobs yet

## Repository Layout

- `firmware/common/`: shared embedded-safe logic (no heap/no RTTI/no exceptions).
- `examples/nrf52-mocap-node/`: intended primary platform path for nRF52.
- `examples/esp32s3-mocap-node/`: secondary or legacy ESP32-S3 platform path.
- `tests/`: host-side TDD suite.
- `tools/`: repo-local scripts/toolchain helpers.
- `datasheets/`: datasheets, reference manuals, and document index.

## Datasheets And Manuals

See:

- `datasheets/INDEX.md`
- `datasheets/README.md`
- `docs/NRF52_SELECTION.md`

## Platform Direction

The project direction is:

- primary MCU family target: `nRF52`
- current automated validation target: host simulation and host tests
- ESP32-S3 path: retained as a secondary or experimental platform path

## Sensor Assembly

Recommended wiring for the current dual-I2C architecture:

- `I2C0` (IMU only, `LSM6DSO`) on chosen SDA/SCL pins
- `I2C1` (shared `BMM350` + `LPS22DF`) on chosen SDA/SCL pins
- Power/common:
  - MCU `3V3` -> all sensor `VCC`
  - MCU `GND` -> all sensor `GND`

Notes:
- Use 3.3 V sensor breakouts only.
- Ensure each I2C bus has pull-ups (many dev boards already do).
- Keep bus wiring short for stable high-rate sampling.
- Configure pins in the chosen platform board support.
- Keep the common runtime platform-agnostic as long as possible.

## Sensor Bring-Up Checklist

1. Confirm each sensor responds on each configured I2C bus.
2. Verify `imu.init()`, `mag.init()`, and `baro.init()` succeed (otherwise app remains in fault loop).
3. Confirm telemetry/log output from `app_main` and sensor bring-up code.
4. Optionally implement:
   - `xiao_mocap_calibration_command(...)`
   - `xiao_mocap_sync_anchor(...)`
   - `xiao_mocap_health_sample(...)`

## On-Target Validation

- Checklist: `docs/validation/ON_TARGET_VALIDATION.md`
- Test log template: `docs/validation/TEST_LOG_TEMPLATE.md`
