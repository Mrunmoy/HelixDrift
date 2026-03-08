# Copilot Instructions for HelixDrift / SensorFusion

## Repository Layout

This repo is an embedded sensor fusion monorepo. The core library lives in `external/SensorFusion/` (a git submodule). The top-level `src/` directory is reserved for a future application layer. `third_party/esp-idf/` contains the Espressif IDF used for ESP32 cross-compilation.

Work for drivers, middleware, tests, and platforms is done inside `external/SensorFusion/`.

## Build & Test Commands

All commands run from `external/SensorFusion/`:

```bash
# Configure and build (host, with tests)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run all tests
./build/test/driver_tests

# Run a single test suite via CTest filter
ctest --test-dir build --output-on-failure -R test_mpu6050

# Run a single GoogleTest case
./build/test/driver_tests --gtest_filter='MPU6050Test.InitSuccess'

# Library-only build (no test binary)
cmake -B build -DSENSORFUSION_BUILD_TESTS=OFF
cmake --build build --parallel

# Cross-compile for a specific platform backend
cmake -B build -DSENSORFUSION_PLATFORM=esp32
cmake --build build --parallel

# Generate code size report
scripts/generate_size_report.sh build _site
```

## Architecture

Five-layer stack (bottom to top):

```
HAL Interfaces (drivers/hal/)        — platform-agnostic I/O contracts
Sensor Drivers (drivers/)            — 17 sensor drivers implementing HAL
Middleware (middleware/)             — fusion, codec, kinematics, ECG
Platform Backends (platform/)        — concrete HAL for esp32 / nrf52 / stm32
Application / Examples               — examples/ and main/
```

### Key modules

| Directory | CMake target | Purpose |
|-----------|-------------|---------|
| `drivers/hal/` | — | Abstract interfaces: `II2CBus`, `ISPIBus`, `IAdcChannel`, `IGpioInterrupt`, `IGpioInput`, `IGpioOutput`, `IDelayProvider`, `INvStore` |
| `drivers/` | `sensorfusion_drivers` | 17 sensor drivers (MPU6050, LSM6DSO, QMC5883L, BMM350, BMP180, LPS22DF, SHT40, SGP40, AD8232, BQ25101, …) |
| `middleware/ahrs/` | `sensorfusion_middleware` | `MahonyAHRS` — 9-DOF complementary filter → quaternion |
| `middleware/hub/` | — | `SensorHub` — multi-sensor read dispatcher with calibration |
| `middleware/codec/` | — | `FrameCodec` (CRC-16 CCITT), `MocapBleTransport`, `TimestampSync` |
| `middleware/motion/` | — | Mocap pipeline: linear accel, forward kinematics, ZUPT |
| `middleware/ecg/` | — | `HeartRateDetector` (Pan-Tompkins) |
| `middleware/altitude/` | — | `AltitudeEstimator` — barometer + accel complementary filter |
| `drivers/calibration/` | — | `CalibrationStore`, `CalibrationFitter` |
| `drivers/nvstore/` | — | `AT24CxxNvStore`, `WearLevelingNvStore` |
| `platform/{esp32,nrf52,stm32}/` | — | Concrete HAL implementations |
| `test/` | `driver_tests` | 428 GoogleTest unit tests |

## Coding Conventions

**Language**: C++17. No exceptions — use `bool` return values for error propagation.

**Namespace**: All library code lives in `namespace sf {}`. Test mocks use `namespace sf::test {}`.

**Naming**:
- Classes/types: `PascalCase` (`MPU6050`, `MahonyAHRS`, `CalibrationStore`)
- Methods/functions: `camelCase` (`readAccel`, `getQuaternion`)
- Private members: trailing underscore (`bus_`, `delay_`, `cfg_`, `q_`)
- Constants / register addresses: `UPPER_SNAKE_CASE` (`PWR_MGMT_1`, `WHO_AM_I`)
- Enum classes: `PascalCase` variants (`enum class AccelRange { G2, G4, G8, G16 }`)

**File layout**:
- One class per file: `ClassName.hpp` / `ClassName.cpp`
- Headers use `#pragma once`
- Register address constants go in an unnamed namespace at the top of the `.cpp`

**Typical driver shape**:
```cpp
// DriverName.hpp
#pragma once
#include "II2CBus.hpp"
#include "IDelayProvider.hpp"
namespace sf {
    struct DriverConfig { /* default-initialized fields */ };
    class DriverName {
    public:
        DriverName(II2CBus& bus, IDelayProvider& delay, const DriverConfig& cfg = {});
        bool init();
        bool readData(DataType& out);
    private:
        II2CBus& bus_;
        IDelayProvider& delay_;
        DriverConfig cfg_;
    };
} // namespace sf

// DriverName.cpp
namespace { constexpr uint8_t REG_WHO_AM_I = 0x0F; /* ... */ }
```

**Data types**:
- `uint8_t` — register addresses and raw byte values
- `int16_t` — raw sensor ADC output before conversion
- `float` — calibrated physical output (g, °/s, µT, hPa)

**Error handling**: guard-clause style — return `false` immediately on bus errors; no deep nesting.

**Formatting**: 4-space indentation, braces on the same line as control statements.

## Testing Conventions

Tests live in `test/test_<module>.cpp`. HAL mocks are in `test/mocks/` (`MockI2CBus.hpp`, `MockDelayProvider.hpp`, etc.).

Typical test fixture:
```cpp
class MPU6050Test : public ::testing::Test {
protected:
    sf::test::MockI2CBus bus;
    sf::test::MockDelayProvider delay;
};
TEST_F(MPU6050Test, InitSuccess) { /* EXPECT_CALL then assert */ }
```

Every driver or middleware addition needs tests covering: init success, init failure (bus error), data read correctness, and conversion/math accuracy.

## Platform Integration

To integrate `SensorFusion` into an application (e.g., the ESP32-S3 example in `examples/esp32s3-mocap-node/`):

```cmake
add_subdirectory(external/SensorFusion)
target_link_libraries(my_app PRIVATE sensorfusion_drivers sensorfusion_middleware)
```

Implement the HAL interfaces for your target in `platform/<target>/` and pass concrete instances to driver constructors.

## Commit Style

Imperative, scoped summaries:
```
Add portable LPS22DF barometric pressure driver with 11 tests
Fix MPU6050 WHO_AM_I mask for variant detection
Refactor SensorHub to use calibration pipeline
```

Include test evidence in PRs: the exact command run and pass count.
