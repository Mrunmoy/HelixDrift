# HelixDrift

Simulation-first mocap node firmware workspace for the `nRF52` integration path.

> Intended primary MCU target: nRF52840 / nRF52
> Current automated validation target: host simulation, host tests, and nRF-oriented firmware builds

## Quick Start

```bash
./build.py --clean --host-only -t
```

This runs the full host-side build and test suite, including simulator-backed
integration tests.

## Build Commands

- `./build.py --host-only -t`: host build and tests only.
- `./build.py --nrf-only`: nRF firmware build only.
- `./build.py --clean`: clean then build.
- `./magic.sh`: convenience wrapper for host tests + nRF build.

`build.py` auto-initializes submodules and calls platform tooling when needed.

## CI

GitHub Actions workflow: `.github/workflows/ci.yml`

- Host unit job: builds host targets and runs `./build/host/helix_tests`
- Host integration job: builds host targets and runs `./build/host/helix_integration_tests`
- There are no automated platform runtime or platform smoke jobs yet

## Repository Layout

- `firmware/common/`: shared embedded-safe logic (no heap/no RTTI/no exceptions).
- `examples/nrf52-mocap-node/`: intended primary platform path for nRF52.
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
- next platform work: nRF52840 bring-up against the already-validated simulation contracts
- currently available physical bring-up board: `nRF52 DK (nRF52832)` for generic
  runtime / flash / OTA-path validation, without changing the intended product
  target

## Current Hardware Bring-Up Targets

- `nrf52_blinky`: generic nRF blinky using the XIAO-style linker layout
- `nrf52dk_blinky`: board-correct LED heartbeat for the Nordic nRF52 DK
- `nrf52dk_bringup`: board-correct UART + LED bring-up image for the Nordic
  nRF52 DK

Artifacts are emitted automatically under `build/nrf/` as:

- `.bin`
- `.hex`
- map files

## Flashing With OpenOCD

Use the repo-local helper from `nix develop`:

```bash
nix develop --command bash -lc 'tools/nrf/flash_openocd.sh build/nrf/nrf52dk_blinky.hex'
```

Or flash the DK bring-up target:

```bash
nix develop --command bash -lc 'tools/nrf/flash_openocd.sh build/nrf/nrf52dk_bringup.hex'
```

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
