# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HelixDrift is an ESP32-S3 motion capture (mocap) node firmware workspace targeting the Seeed XIAO ESP32S3 board. The architecture separates embedded-safe shared logic from hardware-specific ESP-IDF glue, enabling TDD on the host.

## Build Commands

All builds run inside the Nix dev shell (`flake.nix` provides cmake, ninja, gcc, gcc-arm-embedded).

| Command | Effect |
|---|---|
| `./magic.sh` | Bootstrap ESP-IDF + run host tests + ESP32-S3 build (one-shot setup) |
| `./build.py --host-only -t` | Host build + run GoogleTest suite |
| `./build.py --host-only` | Host build only, no tests |
| `./build.py --esp32s3-only` | ESP32-S3 firmware build |
| `./build.py --clean` | Wipe `build/` then run full build |

`build.py` auto-initializes required git submodules before building.

### Running a single host test

```bash
nix develop --command bash -lc "ctest --test-dir build/host -R <TestName> --output-on-failure"
```

### Flashing to device

```bash
source third_party/esp-idf/export.sh
idf.py -C examples/esp32s3-mocap-node -B build/esp32s3 -p /dev/ttyACM0 flash monitor
```

## Architecture

### Key directories

- `firmware/common/` — shared, embedded-safe C++ (no heap/no RTTI/no exceptions). All new logic goes here first.
- `examples/esp32s3-mocap-node/src/` — ESP-IDF target glue: `board_xiao_esp32s3.*`, `Esp32OtaFlashBackend.*`.
- `tests/` — host GoogleTest suite (one `test_*.cpp` per translation unit).
- `tests/mocks/` — GMock doubles (e.g., `MockOtaFlashBackend.hpp`).
- `tools/esp/stubs/include/` — minimal ESP-IDF API stubs for host builds.
- `external/SensorFusion` — git submodule: codec, HAL, and middleware for sensor data.
- `third_party/esp-idf` — ESP-IDF git submodule.

### Host/target abstraction pattern

ESP32-specific code on the host compiles with `-DESP32_STUB` and uses stubs from `tools/esp/stubs/include/` instead of real ESP-IDF headers. Production code is gated behind interface abstractions (e.g., `EspOtaOpsInterface`) so tests can inject mocks without touching hardware APIs.

When adding ESP-IDF-dependent code:
1. Define an interface in `firmware/common/` (pure virtual or template).
2. Implement the real version in `examples/esp32s3-mocap-node/src/`.
3. Add a stub/mock in `tests/mocks/` for host tests.
4. Guard any new stubs with `if(ESP32_STUB)` in `CMakeLists.txt`.

### SensorFusion submodule rule

If a fix belongs to `external/SensorFusion`, commit and push it in the SensorFusion repo first, then update the submodule pointer here.

## CI

`.github/workflows/ci.yml` runs on every push and PR:
- **Gate job**: `./build.py --host-only -t` (must pass before merges).
- **Smoke matrix**: separate `host` and `esp32s3` jobs; ESP32-S3 artifacts are uploaded.

## On-Target Validation

- Checklist: `docs/validation/ON_TARGET_VALIDATION.md`
- Test log template: `docs/validation/TEST_LOG_TEMPLATE.md`
