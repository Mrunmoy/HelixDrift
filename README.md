# HelixDrift

ESP32-S3 mocap node firmware workspace.

> Branch: `esp32s3-xiao`  
> Target board: Seeed XIAO ESP32S3

## Quick Start

```bash
./magic.sh
```

This bootstraps ESP-IDF, runs host tests, and builds the ESP32-S3 project.

## Build Commands

- `./build.py --host-only -t`: host build and tests only.
- `./build.py --esp32s3-only`: ESP32-S3 build only.
- `./build.py --clean`: clean then build.
- `./magic.sh`: one-command setup + test + ESP32-S3 build.

`build.py` auto-initializes submodules and calls ESP-IDF tooling when needed.

## CI

GitHub Actions workflow: `.github/workflows/ci.yml`

- Gate job: runs `./build.py -t` on every push and pull request.
- Smoke matrix:
  - `host`: `./build.py --host-only -t`
  - `nrf`: `./build.py --nrf-only` (legacy branch compatibility)

## Repository Layout

- `firmware/common/`: shared embedded-safe logic (no heap/no RTTI/no exceptions).
- `examples/esp32s3-mocap-node/`: ESP-IDF project for XIAO ESP32S3.
- `tests/`: host-side TDD suite.
- `tools/`: repo-local scripts/toolchain helpers.
- `datasheets/`: datasheets, reference manuals, and document index.

## Datasheets And Manuals

See:

- `datasheets/INDEX.md`
- `datasheets/README.md`

## Hardware Setup (Seeed XIAO ESP32S3 + Sensor Dev Boards)

Recommended wiring for the current dual-I2C architecture on this branch:

- `I2C0` (IMU only, `LSM6DSO`) on chosen SDA/SCL pins
- `I2C1` (shared `BMM350` + `LPS22DF`) on chosen SDA/SCL pins
- Power/common:
  - XIAO `3V3` -> all sensor `VCC`
  - XIAO `GND` -> all sensor `GND`

Notes:
- Use 3.3 V sensor breakouts only.
- Ensure each I2C bus has pull-ups (many dev boards already do).
- Keep bus wiring short for stable high-rate sampling.
- Configure pins in your ESP-IDF project (`menuconfig` / board header).
- Reference board docs: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

## Flashing XIAO ESP32S3

From ESP-IDF build output:

- `build/esp32s3/esp32s3_mocap_node.bin`

Flash over USB:

```bash
source third_party/esp-idf/export.sh
idf.py -C examples/esp32s3-mocap-node -B build/esp32s3 -p /dev/ttyACM0 flash monitor
```

Reference: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/

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
