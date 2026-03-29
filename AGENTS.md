# HelixDrift AGENTS.md

AI coding agent guidance for the HelixDrift project.

## Project Overview

HelixDrift is an ESP32-S3 motion capture (mocap) node firmware workspace targeting the Seeed XIAO ESP32S3 board. The project implements a wireless IMU-based motion tracking system that streams orientation quaternions via Bluetooth Low Energy (BLE).

**Key Features:**
- Dual-I2C sensor architecture (IMU on I2C0, magnetometer + barometer on I2C1)
- 50 Hz output in performance mode / 40 Hz in battery mode
- OTA (Over-The-Air) firmware updates via BLE
- Timestamp synchronization for multi-node setups
- Health telemetry (battery, link quality, calibration state)

**Target Hardware:**
- MCU: ESP32-S3 (Seeed XIAO ESP32S3)
- IMU: LSM6DSO
- Magnetometer: BMM350
- Barometer: LPS22DF

## Technology Stack

**Languages:** C++17 (embedded), C (ESP-IDF integration)

**Build Systems:**
- CMake 3.20+ (host builds and orchestration)
- ESP-IDF (xtensa-esp32s3-elf-gcc toolchain)
- Ninja (host builds)

**Dependencies:**
- ESP-IDF v5.x (submodule in `third_party/esp-idf`)
- SensorFusion library (submodule in `external/SensorFusion`)
- GoogleTest v1.14.0 (fetched via CMake FetchContent)
- GoogleMock (for test doubles)

**Development Environment:**
- Nix flake for reproducible host toolchain
- Python 3 (build orchestration)

## Directory Structure

```
.
├── firmware/common/          # Platform-agnostic embedded code
│   ├── BlinkEngine.cpp/hpp   # LED blink pattern engine
│   ├── MocapNodeLoop.hpp     # Main mocap timing loop (template)
│   ├── MocapProfiles.hpp     # Performance/battery mode profiles
│   ├── MocapBleSender.cpp/hpp  # BLE transport abstraction
│   ├── MocapHealthTelemetry.hpp # Health telemetry framing
│   ├── TimestampSynchronizedTransport.hpp # Clock sync
│   └── ota/                  # OTA update subsystem
│       ├── OtaManager.cpp/hpp    # OTA state machine
│       ├── BleOtaService.cpp/hpp # BLE GATT service handler
│       ├── OtaFlashBackend.hpp   # Flash backend interface
│       ├── IOtaManager.hpp       # Abstract OTA interface
│       └── EspOtaOpsInterface.hpp # ESP-IDF OTA wrapper
│
├── examples/
│   ├── esp32s3-mocap-node/   # ESP-IDF project (main target)
│   │   ├── main/main.c       # Application entry point
│   │   ├── src/              # Board-specific implementation
│   │   ├── sdkconfig.defaults # ESP-IDF configuration
│   │   └── partitions_ota.csv # Flash partition layout
│   ├── nrf52-blinky/         # nRF52 blinky example (legacy)
│   └── nrf52-mocap-node/     # nRF52840 mocap node (legacy)
│
├── tests/                    # Host-side unit tests
│   ├── test_*.cpp            # Test suites
│   └── mocks/                # GoogleMock test doubles
│
├── tools/                    # Build/toolchain helpers
│   ├── esp/setup_idf.sh      # ESP-IDF bootstrap script
│   └── esp/stubs/            # Host stubs for ESP-IDF APIs
│
├── docs/validation/          # Validation checklists
├── datasheets/               # Hardware reference docs
├── third_party/              # Git submodules (ESP-IDF)
└── external/                 # Git submodules (SensorFusion)
```

## Build System

### Quick Start

```bash
# One-command setup, test, and build
./magic.sh
```

### Build Commands

```bash
# Host build with tests
./build.py --host-only -t

# ESP32-S3 firmware build only
./build.py --esp32s3-only

# Clean build
./build.py --clean

# Full build (host + ESP32-S3)
./build.py
```

### Build Process

1. **Host Build:**
   - Uses Nix flake for toolchain isolation
   - CMake with Ninja generator
   - Compiles all `firmware/common/` code as static libraries
   - Links GoogleTest and runs unit tests via CTest

2. **ESP32-S3 Build:**
   - Bootstraps ESP-IDF via `tools/esp/setup_idf.sh`
   - Uses ESP-IDF's CMake build system
   - Cross-compiles for xtensa-esp32s3-elf target

### Flashing

```bash
source third_party/esp-idf/export.sh
idf.py -C examples/esp32s3-mocap-node -B build/esp32s3 -p /dev/ttyACM0 flash monitor
```

## Testing Strategy

### Philosophy
- **Test-First Development:** Write tests before implementation
- **Off-Target Testing:** All logic tested on host (x86) before hardware
- **Mock-Based:** Hardware dependencies mocked using GoogleMock

### Test Organization

| Test File | Coverage |
|-----------|----------|
| `test_blink_engine.cpp` | LED blink timing and state machine |
| `test_mocap_node_loop.cpp` | Main loop cadence (50/40 Hz contracts) |
| `test_mocap_profiles.cpp` | Performance/battery mode constants |
| `test_mocap_ble_sender.cpp` | BLE transport abstraction |
| `test_mocap_health_telemetry.cpp` | Health frame encoding |
| `test_timestamp_synchronized_transport.cpp` | Clock synchronization |
| `test_ota_manager.cpp` | OTA state machine and CRC verification |
| `test_ble_ota_service.cpp` | BLE GATT service protocol |
| `test_esp32_ota_flash_backend.cpp` | ESP-IDF flash backend |

### Running Tests

```bash
./build.py --host-only -t
```

This builds the host target and runs all tests via CTest with `--output-on-failure`.

### Test Patterns

1. **Template-Based Testing:** Components use templates for dependency injection:
   ```cpp
   template <typename Clock, typename Pipeline, typename Transport, typename Sample>
   class MocapNodeLoopT { ... };
   ```

2. **Fake Objects:** Tests use lightweight fakes:
   ```cpp
   struct FakeClock {
       uint64_t now = 0;
       uint64_t nowUs() const { return now; }
   };
   ```

3. **GoogleMock:** For interface-based mocking:
   ```cpp
   class MockOtaFlashBackend : public OtaFlashBackend {
       MOCK_METHOD(bool, eraseSlot, (), (override));
       MOCK_METHOD(bool, writeChunk, (uint32_t, const uint8_t*, size_t), (override));
   };
   ```

## Code Style Guidelines

### Language Constraints (firmware/common/)

Code in `firmware/common/` must be **embedded-safe**:

- ✅ C++17 features: templates, constexpr, auto
- ✅ Standard headers: `<cstdint>`, `<cstddef>`
- ❌ No dynamic memory allocation (no `new`/`delete`)
- ❌ No RTTI (`typeid`, `dynamic_cast`)
- ❌ No exceptions (`throw`, `try`/`catch`)
- ❌ No STL containers that allocate (`std::vector`, `std::string`)
- ❌ No `iostream`, `stdio.h` in library code

### Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Namespaces | `snake_case` | `namespace helix { ... }` |
| Classes | `PascalCase` | `class OtaManager { ... }` |
| Functions | `camelCase` | `void reset()` |
| Private members | trailing underscore | `uint32_t onMs_` |
| Constants | `kPascalCase` | `constexpr uint32_t kStatusLen = 6` |
| Macros | `SCREAMING_SNAKE_CASE` | `#define ESP_OK 0` |
| Template parameters | `PascalCaseT` | `typename ClockT` |

### File Organization

- Headers use `#pragma once`
- Interface classes in separate header files
- Implementation in `.cpp` files when non-trivial
- Template implementations in headers

### Include Order

```cpp
#pragma once

// 1. C/C++ standard headers
#include <cstdint>
#include <cstddef>

// 2. Project headers (alphabetical)
#include "FrameCodec.hpp"
#include "OtaFlashBackend.hpp"
```

### Class Design

1. **Interfaces:** Pure virtual base classes for testability
2. **Templates:** Use for compile-time polymorphism (zero overhead)
3. **State Machines:** Explicit enum states, well-documented transitions

Example:
```cpp
enum class OtaState : uint8_t {
    IDLE,       ///< Ready to accept a new transfer
    RECEIVING,  ///< Transfer in progress
    COMMITTED,  ///< Image accepted and pending upgrade
};
```

## Hardware Abstraction

### Weak Symbols Pattern

Platform-specific functions use weak symbols for test override:

```cpp
// In MocapBleSender.cpp
extern "C" bool __attribute__((weak)) sf_mocap_ble_notify(...) {
    return false;  // Default stub
}

// Production provides strong definition
// Tests override with test double
```

### ESP-IDF Stubs

Host builds use stubs in `tools/esp/stubs/include/`:
- `esp_ota_ops.h` - Minimal ESP-IDF OTA types
- Compile with `-DESP32_STUB` to enable stubs
- Error if stubs included in hardware builds (safety check)

## OTA System Architecture

### Components

```
BleOtaService (BLE GATT handler)
        |
        v
+--------------+     +------------------+     +------------------+
| IOtaManager  |---->| OtaManager       |---->| OtaFlashBackend  |
| (interface)  |     | (state machine)  |     | (platform impl)  |
+--------------+     +------------------+     +------------------+
                                                      |
                                        +-------------+-------------+
                                        |                           |
                                        v                           v
                              +------------------+       +------------------+
                              | Esp32OtaFlash    |       | MockOtaFlash     |
                              | Backend          |       | (tests)          |
                              +------------------+       +------------------+
```

### OTA Protocol

Three-characteristic BLE GATT service:

| Characteristic | Direction | Purpose |
|----------------|-----------|---------|
| `OTA_CTRL` | Write | Begin(0x01), Abort(0x02), Commit(0x03) |
| `OTA_DATA` | Write-no-rsp | Sequential chunk writes `[offset:4][payload...]` |
| `OTA_STATUS` | Read/Notify | `[state:1][bytes:4][last_status:1]` |

## Git Submodules

| Path | Repository | Purpose |
|------|------------|---------|
| `external/SensorFusion` | Mrunmoy/SensorFusion | Sensor fusion and frame codec |
| `third_party/esp-idf` | espressif/esp-idf | ESP32-S3 SDK |

**Rule:** If a fix belongs to SensorFusion, fix it there first, push it, then update this repo's submodule pointer.

## CI/CD

GitHub Actions workflow: `.github/workflows/ci.yml`

**Gate Job (every push/PR):**
- Runs `./build.py --host-only -t`
- Must pass before merge

**Smoke Matrix:**
- `host`: Host build + tests
- `esp32s3`: Cross-compilation for ESP32-S3
- Artifacts: Firmware binaries uploaded

## Development Workflow

### Task Completion Checklist

For every task in `TASKS.md`:
- [ ] Tests first (or test update first) and green
- [ ] Off-target build passes
- [ ] Docs updated (README or design page)
- [ ] If SensorFusion changed: commit/push there first, then submodule update here

### Adding New Features

1. Write test in `tests/test_<feature>.cpp`
2. Implement in `firmware/common/`
3. Run `./build.py --host-only -t`
4. Integrate into `examples/esp32s3-mocap-node/`
5. Run `./build.py --esp32s3-only`
6. Update relevant documentation

### Debugging

```bash
# Enable verbose logging in ESP-IDF
idf.py menuconfig
# Component config → Log output → Default log verbosity → Debug
```

## Security Considerations

1. **OTA Integrity:** All firmware images verified with CRC32 before activation
2. **Rollback Protection:** ESP-IDF bootloader rolls back if app doesn't confirm validity
3. **Confirmation Required:** App must call `xiao_ota_confirm_image()` after successful boot

## Reference Documents

- Hardware datasheets: `datasheets/INDEX.md`
- Validation checklist: `docs/validation/ON_TARGET_VALIDATION.md`
- Task tracking: `TASKS.md`
