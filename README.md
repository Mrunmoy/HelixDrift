# HelixDrift

nRF mocap node firmware workspace (split from SensorFusion library-only repo).

## Quick Start

```bash
./build.py
```

This validates host tests and cross-builds nRF targets. It does not flash hardware.

## Build Commands

- `./build.py`: build all targets (host + nRF cross-build).
- `./build.py -t`: build all + run host tests.
- `./build.py --host-only -t`: host build and tests only.
- `./build.py --nrf-only`: nRF cross-build only.
- `./build.py --clean`: clean then build.

`build.py` auto-initializes `external/SensorFusion` for nRF builds.

## CI

GitHub Actions workflow: `.github/workflows/ci.yml`

- Gate job: runs `./build.py -t` on every push and pull request.
- Smoke matrix:
  - `host`: `./build.py --host-only -t`
  - `nrf`: `./build.py --nrf-only`
- nRF smoke job uploads:
  - `nrf52_blinky` + `nrf52_mocap_node` (ELF)
  - `nrf52_blinky.hex` + `nrf52_mocap_node.hex`

## Repository Layout

- `firmware/common/`: shared embedded-safe logic (no heap/no RTTI/no exceptions).
- `examples/nrf52-blinky/`: baseline nRF blinky firmware app.
- `examples/nrf52-mocap-node/`: nRF mocap node firmware app using SensorFusion submodule.
- `tests/`: host-side TDD suite.
- `tools/`: repo-local toolchain and nRF stub headers for off-target validation.
- `datasheets/`: datasheets, reference manuals, and document index.

## Datasheets And Manuals

See:

- `datasheets/INDEX.md`
- `datasheets/README.md`

## Next Steps

- Wire node calibration command flow and timestamp sync with central node protocol.
- Add battery/health telemetry frames and CI gates for host + nRF smoke builds.

## nRF BLE Integration Boundary

- `examples/nrf52-mocap-node/src/board_xiao_nrf52840.cpp` provides the board-facing bridge.
- Implement these weak board hooks in your BLE/control layer:
  - `xiao_ble_stack_notify(const uint8_t* data, size_t len)`
  - `xiao_mocap_calibration_command(void)` returning:
  - `0`: none
  - `1`: capture stationary
  - `2`: capture T-pose
  - `3`: reset calibration
- `xiao_mocap_sync_anchor(uint64_t* localUs, uint64_t* remoteUs)` to feed central-time sync anchors.
- `xiao_mocap_health_sample(...)` to publish battery/link/drop telemetry; these are emitted as `NODE_HEALTH` frames.
- `examples/nrf52-mocap-node` uses `WeakSymbolBleSender` + `MocapNodeLoopT` to keep the loop testable off-target.

## Hardware Setup (Seeed XIAO nRF52840 + Sensor Dev Boards)

Recommended wiring for the current dual-I2C architecture:

- `TWIM0` (IMU only, `LSM6DSO`)
  - XIAO `D4/P0.04` -> `LSM6DSO SDA`
  - XIAO `D5/P0.05` -> `LSM6DSO SCL`
- `TWIM1` (shared `BMM350` + `LPS22DF`)
  - XIAO `D6/P1.11` -> `BMM350 SDA` and `LPS22DF SDA`
  - XIAO `D7/P1.12` -> `BMM350 SCL` and `LPS22DF SCL`
- Power/common:
  - XIAO `3V3` -> all sensor `VCC`
  - XIAO `GND` -> all sensor `GND`

Notes:
- Use 3.3 V sensor breakouts only.
- Ensure each I2C bus has pull-ups (many dev boards already do).
- Keep bus wiring short for stable high-rate sampling.
- `xiao_board_init_i2c()` now initializes `TWIM0` and `TWIM1` for this wiring map in `board_xiao_nrf52840.cpp`.
- Reference pin map: https://wiki.seeedstudio.com/XIAO_BLE/

## Flashing XIAO nRF52840

Current repo output for the mocap app:

- ELF: `build/nrf/nrf52_mocap_node`

Create HEX/BIN from ELF:

```bash
arm-none-eabi-objcopy -O ihex build/nrf/nrf52_mocap_node build/nrf/nrf52_mocap_node.hex
arm-none-eabi-objcopy -O binary build/nrf/nrf52_mocap_node build/nrf/nrf52_mocap_node.bin
```

### SWD (recommended for this repo right now)

Use a J-Link (or compatible probe) connected to XIAO SWD pads:

```bash
nrfjprog --program build/nrf/nrf52_mocap_node.hex --chiperase --verify --reset
```

### UF2 bootloader mode (USB)

- Double-press reset to enter UF2 mode (mass-storage drive appears).
- This repo does not yet generate UF2 directly; add a UF2 conversion step in your board-specific workflow if you want drag-and-drop flashing.
- UF2 mode reference: https://wiki.seeedstudio.com/XIAO-nRF52840-Zephyr-RTOS/

## Sensor Bring-Up Checklist

1. Confirm each sensor responds on its assigned bus.
2. Verify `imu.init()`, `mag.init()`, and `baro.init()` succeed (otherwise app remains in fault loop).
3. Implement `xiao_ble_stack_notify(...)` and confirm quaternion notifications at target cadence.
4. Optionally implement:
   - `xiao_mocap_calibration_command(...)`
   - `xiao_mocap_sync_anchor(...)`
   - `xiao_mocap_health_sample(...)`

## On-Target Validation

- Checklist: `docs/validation/ON_TARGET_VALIDATION.md`
- Test log template: `docs/validation/TEST_LOG_TEMPLATE.md`
