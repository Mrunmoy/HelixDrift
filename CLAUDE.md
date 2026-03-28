# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

HelixDrift is ESP32-S3 motion capture node firmware for the Seeed XIAO ESP32S3. It streams IMU orientation quaternions over BLE at 50 Hz (performance) or 40 Hz (battery mode) using dual-I2C sensors (LSM6DSO, BMM350, LPS22DF). Includes OTA firmware updates via BLE and multi-node timestamp synchronization.

## Build Commands

```bash
./build.py --host-only -t      # Host build + run all tests (gate CI job)
./build.py --esp32s3-only      # Cross-compile for ESP32-S3
./build.py --clean             # Clean then full build
./magic.sh                     # One-command: bootstrap + test + build
```

Host builds use Nix flake for toolchain isolation (CMake + Ninja). ESP32-S3 builds use ESP-IDF. `build.py` auto-initializes git submodules.

### Running Individual Tests

```bash
./build.py --host-only -t                        # Runs all via CTest
./build-arm/helix_tests                          # Unit tests binary
./build-arm/helix_integration_tests              # Integration tests binary
./build-arm/helix_tests --gtest_filter='TestSuite.TestName'  # Single test
```

### Flashing

```bash
source third_party/esp-idf/export.sh
idf.py -C examples/esp32s3-mocap-node -B build/esp32s3 -p /dev/ttyACM0 flash monitor
```

## Architecture

### Core Firmware (`firmware/common/`)

Platform-agnostic, embedded-safe C++17. No heap, no RTTI, no exceptions, no STL containers that allocate.

Key components:
- **MocapNodeLoop** (template) - Main timing loop with compile-time DI for Clock, Pipeline, Transport, Sample
- **OTA subsystem** (`ota/`) - Three-tier: BleOtaService (GATT) -> OtaManager (state machine) -> OtaFlashBackend (platform)
- **BlinkEngine** - LED pattern state machine
- **MocapBleSender** - BLE transport using weak symbols for test override

### Simulators (`simulators/`)

Integration testing framework with virtual I2C bus, sensor simulators (LSM6DSO, BMM350, LPS22DF), virtual gimbal, and EEPROM simulation. Tests sensor fusion pipeline end-to-end on host.

### ESP-IDF Project (`examples/esp32s3-mocap-node/`)

Board-specific integration: `main.c` entry point, board init, OTA flash backend implementation.

### Git Submodules

- `external/SensorFusion` - Sensor fusion + frame codec. **Rule:** fix bugs there first, push, then update submodule pointer here.
- `third_party/esp-idf` - ESP-IDF SDK.

## Testing

TDD-first: write tests before implementation. All logic tested off-target (x86) before hardware.

- **Unit tests** (`tests/`) - GoogleTest/GoogleMock. Template-based DI with lightweight fakes for clocks, pipelines, etc.
- **Integration tests** (`simulators/tests/`) - Full sensor fusion pipeline with virtual sensors.
- **CI gate** - `./build.py --host-only -t` must pass on every push/PR.

## Code Conventions

### Embedded Constraints (`firmware/common/`)

Allowed: C++17 templates, constexpr, auto, `<cstdint>`, `<cstddef>`. Forbidden: `new`/`delete`, RTTI, exceptions, `std::vector`/`std::string`, iostream.

### Naming

| Element | Style | Example |
|---------|-------|---------|
| Classes | `PascalCase` | `OtaManager` |
| Functions | `camelCase` | `reset()` |
| Private members | trailing `_` | `onMs_` |
| Constants | `kPascalCase` | `kStatusLen` |
| Namespaces | `snake_case` | `helix` |
| Template params | `PascalCaseT` | `ClockT` |

### File Style

- `#pragma once` for header guards
- Include order: C/C++ standard headers, then project headers (alphabetical)
- Interfaces as pure virtual base classes in separate headers
- State machines use explicit `enum class` with documented transitions

### Hardware Abstraction

- Weak symbols (`__attribute__((weak))`) for platform functions that tests override
- ESP-IDF stubs in `tools/esp/stubs/include/` for host builds (compiled with `-DESP32_STUB`)

## Development Workflow

1. Write test in `tests/test_<feature>.cpp`
2. Implement in `firmware/common/`
3. `./build.py --host-only -t`
4. Integrate into `examples/esp32s3-mocap-node/`
5. `./build.py --esp32s3-only`
