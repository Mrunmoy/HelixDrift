# HelixDrift nRF-xiao-nrf52840 Branch - Complete Analysis for MCUBoot OTA Implementation

Generated: 2024-03-08
Branch: `nrf-xiao-nrf52840` (HEAD at commit `2bd049a`)
Target Board: Seeed XIAO nRF52840 (nRF52840 SoC)

---

## 1. REPOSITORY STRUCTURE (Full Tree)

```
HelixDrift/
├── README.md                    # Main documentation
├── TASKS.md                     # Execution backlog
├── CMakeLists.txt              # Root build config (CMake 3.20+)
├── build.py                    # Python build orchestrator
├── flake.nix                   # Nix dev environment (nixos-24.11)
├── .github/
│   ├── workflows/
│   │   └── ci.yml              # GitHub Actions CI (host + nRF cross-build matrix)
│   └── copilot-instructions.md
├── cmake/
│   └── (toolchain support files)
├── tools/
│   ├── nrf/
│   │   └── stubs/include/      # nRF SDK header stubs for off-target builds
│   │       ├── NrfSdk.hpp      # Umbrella include (references actual SDK)
│   │       ├── nrfx_twim.h     # Dual-I2C TWIM stubs
│   │       ├── nrfx_spim.h     # SPI stub
│   │       ├── nrfx_saadc.h    # ADC stub
│   │       ├── nrf_gpio.h      # GPIO stub
│   │       ├── nrf_delay.h     # Delay stub
│   │       ├── app_timer.h     # Timer stub
│   │       └── fds.h           # Flash data storage stub
│   ├── toolchains/
│   │   └── arm-none-eabi-gcc.cmake  # Cross-compiler config
│   └── README.md
├── firmware/common/             # Shared embedded-safe libraries
│   ├── BlinkEngine.{hpp,cpp}   # LED blink timing engine
│   ├── MocapBleSender.{hpp,cpp} # BLE sender interface + weak symbol binding
│   ├── MocapHealthTelemetry.hpp # Battery/link quality telemetry structs
│   ├── MocapNodeLoop.hpp        # Main mocap node loop template
│   ├── MocapProfiles.hpp        # Power profiles (IDLE, PERFORMANCE)
│   └── TimestampSynchronizedTransport.hpp # Time sync wrapper
├── examples/
│   ├── nrf52-blinky/
│   │   └── src/main.cpp         # Simple GPIO blink demo
│   ├── nrf52-mocap-node/        # PRIMARY APPLICATION
│   │   └── src/
│   │       ├── main.cpp         # Main mocap node loop (motion capture app)
│   │       ├── board_xiao_nrf52840.hpp    # Board interface headers
│   │       └── board_xiao_nrf52840.cpp    # I2C init + weak symbol implementations
│   └── esp32s3-mocap-node/      # (different branch)
├── external/
│   └── SensorFusion/            # Git submodule (v1.1.0)
│       ├── VERSION              # v1.1.0
│       ├── CMakeLists.txt       # SensorFusion build config
│       ├── drivers/             # Sensor drivers (LSM6DSO, BMM350, LPS22DF, etc.)
│       ├── middleware/          # Motion capture codec, frames, calibration
│       ├── platform/
│       │   ├── nrf52/           # nRF52 platform layer (TWIM, GPIO, Delay, FDS store)
│       │   ├── esp32/           # ESP32 platform layer
│       │   └── stm32/           # STM32 platform layer
│       └── test/                # Host-side TDD tests
├── tests/                       # HelixDrift-level host tests
│   ├── test_blink_engine.cpp
│   ├── test_mocap_ble_sender.cpp
│   ├── test_mocap_health_telemetry.cpp
│   ├── test_mocap_node_loop.cpp
│   ├── test_timestamp_synchronized_transport.cpp
│   └── test_mocap_profiles.cpp
├── docs/validation/
│   ├── ON_TARGET_VALIDATION.md  # Hardware validation checklist
│   └── TEST_LOG_TEMPLATE.md     # Test result template
├── datasheets/
│   ├── INDEX.md                 # Document index
│   ├── README.md                # Datasheet fetch helper docs
│   └── fetch_datasheets.sh
├── build/                       # Build outputs (generated)
│   ├── host/                    # Host cross-build (gtest)
│   ├── nrf/                     # nRF cross-build
│   │   ├── nrf52_blinky         # Blinky ELF (~52 KB)
│   │   ├── nrf52_blinky.hex
│   │   ├── nrf52_mocap_node     # Mocap node ELF (~52 KB)
│   │   ├── nrf52_mocap_node.hex
│   │   └── (CMake build files)
│   └── esp32s3/                 # ESP32 build (unused on nrf branch)
├── src/                         # Reserved for future app layer
├── third_party/
│   └── esp-idf/                 # Espressif IDF (for ESP32 builds, unused here)
└── .gitmodules
```

---

## 2. BUILD CONFIGURATION FILES

### 2.1 Root CMakeLists.txt
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(helixdrift LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HELIXDRIFT_BUILD_TESTS "Build host tests" ON)
option(HELIXDRIFT_BUILD_NRF_EXAMPLES "Build nRF firmware examples" ON)

# Common libraries
add_library(helix_blink STATIC firmware/common/BlinkEngine.cpp)
add_library(helix_mocap_common STATIC firmware/common/MocapBleSender.cpp)

# nRF examples (conditional)
if(HELIXDRIFT_BUILD_NRF_EXAMPLES)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tools/nrf/stubs/include)
    
    # SensorFusion integration
    set(SENSORFUSION_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SENSORFUSION_BUILD_APP OFF CACHE BOOL "" FORCE)
    set(SENSORFUSION_PLATFORM nrf52 CACHE STRING "" FORCE)
    add_subdirectory(external/SensorFusion)
    
    # Blinky executable
    add_executable(nrf52_blinky examples/nrf52-blinky/src/main.cpp)
    target_link_libraries(nrf52_blinky PRIVATE helix_blink)
    
    # Mocap node executable (PRIMARY)
    add_executable(nrf52_mocap_node
        examples/nrf52-mocap-node/src/main.cpp
        examples/nrf52-mocap-node/src/board_xiao_nrf52840.cpp
    )
    target_link_libraries(nrf52_mocap_node PRIVATE
        helix_mocap_common
        sensorfusion_middleware
        sensorfusion_platform_nrf52
    )
endif()

# Host tests (googletest)
if(HELIXDRIFT_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest GIT_REPOSITORY ... GIT_TAG v1.14.0)
    add_executable(helix_tests tests/*.cpp)
endif()
```

**Key Insights**:
- No Zephyr or nRF Connect SDK present
- Using bare CMake cross-compilation (arm-none-eabi-gcc toolchain)
- SensorFusion library integration with nrf52 platform backend
- Stubs for off-target (host) testing

### 2.2 Cross-Compiler Configuration
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/tools/toolchains/arm-none-eabi-gcc.cmake`

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(COMMON_FLAGS "-mcpu=cortex-m4 -mthumb -ffunction-sections -fdata-sections")
set(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections -specs=nosys.specs")
```

**Key Insights**:
- Cortex-M4 target (nRF52840 = Cortex-M4 @ 64 MHz)
- No exceptions, no RTTI (embedded-safe)
- Garbage collection of unused sections
- Bare-metal linking (no OS stubs)

### 2.3 nRF SDK Platform CMakeLists
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/external/SensorFusion/platform/nrf52/CMakeLists.txt`

```cmake
add_library(sensorfusion_platform_nrf52 STATIC
    NrfTwimBus.cpp       # I2C driver
    NrfSpimBus.cpp       # SPI driver
    NrfGpio.cpp          # GPIO driver
    NrfSaadcChannel.cpp  # ADC driver
    NrfDelay.cpp         # Timing/delay
    NrfFdsStore.cpp      # Flash data store
)

target_include_directories(sensorfusion_platform_nrf52 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sensorfusion_platform_nrf52 PUBLIC
    sensorfusion_drivers
)
```

### 2.4 Nix Development Environment
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/flake.nix`

```nix
{
  description = "HelixDrift nRF mocap node workspace";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
  
  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          git
          python3
          curl
          cmake
          ninja
          gcc                 # Host compiler
          gcc-arm-embedded    # ARM cross-compiler
        ];
      };
    };
}
```

---

## 3. ZEPHYR / nRF CONNECT SDK CONFIGURATION

**⚠️ IMPORTANT: This project does NOT use Zephyr or nRF Connect SDK.**

- No `prj.conf` files found
- No `west.yml` configuration
- No device tree files (`.dts`, `.dtsi`)
- No Zephyr overlay files (`.overlay`)
- No Kconfig files in HelixDrift repo (only in third_party ESP-IDF)

**Instead**:
- Uses **bare CMake + arm-none-eabi-gcc** cross-compilation
- Custom platform layer in `external/SensorFusion/platform/nrf52/`
- SDK headers stubbed for off-target (host) testing

---

## 4. HARDWARE ABSTRACTION / PLATFORM LAYER

The nRF SDK hardware access is abstracted via the SensorFusion platform layer:

### 4.1 nRF Platform Layer Files
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/external/SensorFusion/platform/nrf52/`

| File | Purpose |
|------|---------|
| `NrfSdk.hpp` | Umbrella header pulling in actual SDK (`nrfx_twim.h`, `nrf_gpio.h`, etc.) |
| `NrfTwimBus.cpp/hpp` | I2C (TWIM) bus driver for sensor communication |
| `NrfSpimBus.cpp/hpp` | SPI bus driver |
| `NrfGpio.cpp/hpp` | GPIO control |
| `NrfDelay.cpp/hpp` | Microsecond-precision delay/timestamp |
| `NrfSaadcChannel.cpp/hpp` | Analog-to-digital converter |
| `NrfFdsStore.cpp/hpp` | Flash data storage (NRF Flash Data Store) |

### 4.2 Board-Specific Layer
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/examples/nrf52-mocap-node/src/board_xiao_nrf52840.cpp`

```cpp
// Seeed XIAO nRF52840 pinout
constexpr uint32_t kTwim0SdaPin = 4;   // D4 / P0.04 (LSM6DSO)
constexpr uint32_t kTwim0SclPin = 5;   // D5 / P0.05
constexpr uint32_t kTwim1SdaPin = 43;  // D6 / P1.11 (BMM350 + LPS22DF)
constexpr uint32_t kTwim1SclPin = 44;  // D7 / P1.12

// I2C initialization
bool xiao_board_init_i2c() {
    nrfx_twim_config_t twim0Cfg{};
    twim0Cfg.scl = kTwim0SclPin;
    twim0Cfg.sda = kTwim0SdaPin;
    twim0Cfg.frequency = NRF_TWIM_FREQ_400K;  // 400 kHz I2C
    twim0Cfg.interrupt_priority = 6;
    
    // Similar for TWIM1...
    
    if (nrfx_twim_init(&g_twim0, &twim0Cfg, nullptr, nullptr) != NRFX_SUCCESS) return false;
    // ...
    return true;
}

// Weak symbol hooks for BLE integration
extern "C" bool __attribute__((weak)) xiao_ble_stack_notify(const uint8_t* data, size_t len) {
    return false;  // Implemented by BLE stack
}
extern "C" uint8_t __attribute__((weak)) xiao_mocap_calibration_command() {
    return 0;  // Calibration commands from central
}
extern "C" bool __attribute__((weak)) xiao_mocap_sync_anchor(...) {
    return false;  // Timestamp sync from central
}
extern "C" bool __attribute__((weak)) xiao_mocap_health_sample(...) {
    return false;  // Battery/health telemetry
}
```

---

## 5. MAIN APPLICATION SOURCE

### 5.1 Primary App: nRF Mocap Node
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/examples/nrf52-mocap-node/src/main.cpp`

**Purpose**: Motion capture node firmware for Seeed XIAO nRF52840

**Key Components**:
- **Sensors**:
  - IMU: LSM6DSO (accel + gyro) on TWIM0
  - Magnetometer: BMM350 on TWIM1
  - Barometer: LPS22DF on TWIM1
  
- **Pipeline**:
  ```
  Raw Sensor Data → MocapNodePipeline → Quaternion Fusion
                                      → Calibration Flow
                                      → Timestamp Sync
                                      → BLE Transport
                                      → Health Telemetry
  ```

- **BLE Output**:
  - Quaternion frames at configurable Hz (50 Hz in PERFORMANCE mode)
  - Calibration state frames
  - Health telemetry (battery, link quality, dropped frames)

- **Power Profiles** (defined in `MocapProfiles.hpp`):
  - `IDLE`: Lower ODR, longer battery life
  - `PERFORMANCE`: Higher ODR (208 Hz IMU), lower latency

**Main Loop**:
```cpp
int main() {
    // Initialize I2C buses and sensors
    xiao_board_init_i2c();
    LSM6DSO imu(imuBus, delay, imuCfg);
    BMM350 mag(envBus, delay, magCfg);
    LPS22DF baro(envBus, delay, baroCfg);
    
    if (!imu.init() || !mag.init() || !baro.init()) {
        while (true) delay.delayMs(1000);  // Fault loop if sensor init fails
    }
    
    // Create processing pipeline
    MocapNodePipeline pipeline(imu, &mag, &baro, config);
    
    // Setup BLE transport + timestamp sync + health telemetry
    MocapBleTransport bleTx(...);
    TimestampSync timestampSync{};
    NodeHealthTelemetryEmitterT<BleSenderAdapter> healthTx(...);
    
    // Create main node loop
    MocapNodeLoopT loop(clock, pipeline, syncedTx, loopCfg, calibrationHook);
    
    // Main run loop (non-blocking)
    while (true) {
        uint64_t nowUs = clock.nowUs();
        
        // Emit health telemetry every 1 second
        if (nowUs >= nextHealthTickUs) {
            nextHealthTickUs = nowUs + 1000000ULL;
            uint16_t batteryMv = 0;
            uint8_t batteryPercent = 0;
            // ... call weak symbol to get health data, emit frame
            healthTx.send(kNodeId, timestampSync.toRemoteTimeUs(nowUs), telemetry);
        }
        
        // Process one mocap sample (non-blocking if no data ready)
        if (!loop.tick()) {
            delay.delayMs(1);  // Idle sleep
        }
    }
}
```

### 5.2 Simpler App: Blinky Demo
**Location**: `/mnt/data/sandbox/embedded/HelixDrift/examples/nrf52-blinky/src/main.cpp`

Baseline GPIO blink on pin 13 (LED):
```cpp
int main() {
    nrf_gpio_cfg_output(kLedPin);  // Pin 13
    
    BlinkEngine blink(500, 500, true);  // 500ms on, 500ms off
    
    while (true) {
        const bool newLevel = blink.tick(10);  // 10ms ticks
        if (newLevel) nrf_gpio_pin_set(kLedPin);
        else nrf_gpio_pin_clear(kLedPin);
        nrf_delay_ms(10);
    }
}
```

---

## 6. SDK / RUNTIME VERSION INFORMATION

### 6.1 SensorFusion Library Version
- **Current**: v1.1.0
- **Location**: `external/SensorFusion/` (git submodule)
- **Submodule URL**: `https://github.com/Mrunmoy/SensorFusion.git`
- **Last Update**: Commit `2bd049a` "Update SensorFusion submodule to v1.1.0"

### 6.2 Toolchain Versions
- **CMake**: 3.20+ (specified in root CMakeLists.txt)
- **C++ Standard**: C++17 (required)
- **ARM GCC**: `gcc-arm-embedded` (from Nix: nixos-24.11)
- **Nix Packages**: 
  - python3 (for build.py)
  - ninja (build system)
  - cmake

### 6.3 nRF SDK Integration
- **SDK Type**: Custom stubs + nRFx HAL headers (not full nRF Connect SDK)
- **HAL Headers**:
  - `nrfx_twim.h` (I2C)
  - `nrfx_spim.h` (SPI)
  - `nrf_gpio.h` (GPIO)
  - `nrf_delay.h` (Delays)
  - `app_timer.h` (Timer stubs)
  - `fds.h` (Flash Data Store)
- **Location of stubs**: `tools/nrf/stubs/include/`
- **Implementation**: Full actual SDK headers expected at link/compile time (not included in repo)

### 6.4 Build System
- **Build Generator**: Ninja
- **Cross-Compiler**: arm-none-eabi-gcc (cortex-m4)
- **Build Orchestrator**: Python script `build.py`

---

## 7. GITHUB ACTIONS WORKFLOWS

**Location**: `/mnt/data/sandbox/embedded/HelixDrift/.github/workflows/ci.yml`

### CI Pipeline:

```yaml
name: ci
on: [push, pull_request]

jobs:
  gate:
    name: Gate (build.py -t)
    runs-on: ubuntu-latest
    timeout-minutes: 45
    steps:
      - Checkout (with submodules: false initially)
      - Install Nix
      - Enable Nix cache
      - Run: ./build.py -t  # Full host + nRF build + tests
      - Package artifacts (ELF + HEX files)
      - Upload gate-nrf-binaries artifact

  smoke:
    name: Smoke (${{ matrix.target }})
    strategy:
      matrix:
        target: [host, nrf]
    steps:
      - Checkout
      - Install Nix + cache
      - Host: ./build.py --host-only -t
      - nRF: ./build.py --nrf-only
      - Package nRF artifacts (ELF + HEX)
      - Upload nrf-smoke-binaries artifact
```

**Artifacts Published**:
- `nrf52_blinky` + `nrf52_blinky.hex`
- `nrf52_mocap_node` + `nrf52_mocap_node.hex`

---

## 8. PARTITION MAP / MEMORY LAYOUT FILES

**⚠️ NO PARTITION MAP FILES FOUND**

The current build does not define:
- No `pm_static.yml` (Nordic partition manager)
- No `partition_manager.conf`
- No MCUBoot configuration
- No device tree overlays for memory regions

**Current ELF Characteristics**:
```
File: build/nrf/nrf52_mocap_node
Type: ELF 32-bit LSB executable, ARM, EABI5
Size: 52 KB (unlinked, not stripped)
Linking: Statically linked, no OS symbols
```

**⚠️ FOR MCUBoot OTA IMPLEMENTATION**: You will need to define:
1. Flash partition layout (bootloader + app slots)
2. MCUBoot secondary loader binary
3. Linker script for app offset
4. OTA update management code

---

## 9. BOOTLOADER / OTA RELATED FILES

**Current Status**: NONE FOUND

**Search Results**:
- No `bootloader` directory
- No MCUBoot integration
- No DFU (Device Firmware Update) code
- No OTA (Over-The-Air) infrastructure
- No FOTA (Firmware Over-The-Air) code

**Weak Symbol Hooks for Future Integration**:
- `xiao_ble_stack_notify(data, len)` - BLE transport (could carry OTA packets)
- `xiao_mocap_calibration_command()` - Command interface (could extend for OTA)
- `xiao_mocap_sync_anchor(...)` - Time sync
- `xiao_mocap_health_sample(...)` - Health telemetry

---

## 10. TARGET BOARD DETAILS

### Seeed XIAO nRF52840

| Property | Value |
|----------|-------|
| **MCU** | nRF52840 (Nordic Semiconductor) |
| **CPU** | ARM Cortex-M4 @ 64 MHz |
| **RAM** | 256 KB SRAM |
| **Flash** | 1 MB (typical partitioned for bootloader + app) |
| **Peripherals** | 2x I2C (TWIM), SPI, ADC, GPIO, BLE 5.0 |
| **BLE** | Integrated Bluetooth 5.0 + 802.15.4 |
| **Pin Layout** | See https://wiki.seeedstudio.com/XIAO_BLE/ |
| **USB** | USB-C (power + debugging + UF2 bootloader mode) |
| **Form Factor** | Tiny (13.4mm × 11.2mm) |

### Current Wiring (Mocap Node)

**XIAO Pin Mapping**:
- **TWIM0 (IMU - LSM6DSO)**:
  - D4 / P0.04 → SDA
  - D5 / P0.05 → SCL
  - 400 kHz I2C bus

- **TWIM1 (Magnetometer BMM350 + Barometer LPS22DF)**:
  - D6 / P1.11 → SDA (shared)
  - D7 / P1.12 → SCL (shared)
  - 400 kHz I2C bus

- **Power**:
  - 3V3 → All sensors (no 5V)
  - GND → Common ground
  - Pull-ups typically on dev boards already

- **LED** (blinky example):
  - Pin 13 (GPIO output)

### Flash Bootloader Mode (UF2)
- Double-press RESET → enters UF2 mode (mass-storage USB drive)
- Allows drag-and-drop firmware loading
- Reference: https://wiki.seeedstudio.com/XIAO-nRF52840-Zephyr-RTOS/

---

## 11. DOCUMENTATION

### Main README
**Location**: `README.md`

Covers:
- Quick start: `./build.py`
- Build commands (host-only, nRF-only, with tests)
- CI workflow
- Repository layout
- Hardware setup (wiring diagram)
- Flashing instructions (SWD + UF2)
- Sensor bring-up checklist
- On-target validation

### Task Tracker
**Location**: `TASKS.md`

Current status (as of 2024-03-07):
- ✅ Repo bootstrap (nix, build.py, host/nRF build)
- ✅ TDD blinky baseline
- ✅ SensorFusion submodule integration
- ✅ nRF mocap node compiles
- ✅ Host tests passing
- ✅ BLE sender adapter
- ✅ Mocap app-loop tests
- ✅ Calibration command flow
- ✅ Timestamp sync flow
- ✅ Battery/health telemetry
- ✅ CI workflow + matrix smoke
- ⏳ Documentation quickstart (in progress)

### Validation Checklist
**Location**: `docs/validation/ON_TARGET_VALIDATION.md`

Covers:
- Pre-flash validation
- Flash + boot verification
- Sensor bus bring-up
- Runtime behavior (quaternion stream @ 50 Hz)
- Calibration commands
- Motion quality
- Power + thermal
- Exit criteria

### Hardware / Datasheets
**Location**: `datasheets/`

- `INDEX.md` - Document catalog
- `README.md` - Fetcher helper
- `fetch_datasheets.sh` - Script to download PDFs

---

## 12. BUILD OUTPUT ARTIFACTS

### Current Build Directory
**Location**: `build/nrf/`

| Artifact | Size | Purpose |
|----------|------|---------|
| `nrf52_blinky` | ~52 KB | GPIO blink demo (ELF) |
| `nrf52_blinky.hex` | ~52 KB | HEX format for SWD flashing |
| `nrf52_mocap_node` | ~52 KB | Mocap node app (ELF) |
| `nrf52_mocap_node.hex` | ~52 KB | HEX format for SWD flashing |

**Build Characteristics**:
- ELF 32-bit LSB, ARM EABI5
- Statically linked (no RTOS, no libc dynamic linking)
- Not stripped (symbols present for debugging)
- Cortex-M4 ISA

---

## SUMMARY FOR MCUBoot OTA PLANNING

### Current State
- ✅ Bare-metal CM4 app compilation working
- ✅ Dual I2C sensor stack operational
- ✅ BLE integration points defined (weak symbols)
- ✅ Health telemetry frame format ready
- ❌ No bootloader
- ❌ No partition layout
- ❌ No OTA management
- ❌ No firmware signing/verification
- ❌ No rollback protection

### What You Need to Add for MCUBoot OTA

1. **Bootloader Partition**
   - MCUBoot secondary loader (~50-100 KB)
   - Signature verification + rollback protection
   - Partition layout definition (pm_static.yml or linker script)

2. **Flash Layout** (1 MB typical nRF52840)
   ```
   0x00000000 - 0x0000FFFF: MCUBoot (64 KB)
   0x00010000 - 0x0007FFFF: App slot 1 (448 KB)
   0x00080000 - 0x000EFFFF: App slot 2 (448 KB) [OPTIONAL swap slot]
   0x000F0000 - 0x000FFFFF: NVS/NVRAM (64 KB)
   ```

3. **App Modifications**
   - Image trailer (MCUBoot magic + version)
   - OTA manager (receives packets, writes slot 2)
   - Reboot-to-bootloader trigger
   - Firmware rollback handler

4. **BLE OTA Protocol**
   - Packet format (reuse existing frame codec?)
   - CRC/integrity checking
   - Progress tracking
   - Timeout + retry logic

5. **Configuration**
   - MCUBoot config header
   - Linker script for app offset (0x10000)
   - Signing keys (ed25519 or RSA)

6. **CI Integration**
   - Bootloader build in CI
   - Image signing in CI
   - Artifact generation (mcuboot.bin + app.bin)

### Existing Integration Points
- **BLE Notify Hook**: `xiao_ble_stack_notify()` - perfect for OTA packets
- **Health Telemetry**: Could report OTA progress % in telemetry frames
- **Calibration Command Hook**: Could repurpose or extend for control
- **Weak Symbol Pattern**: Allows decoupling BLE stack from mocap app

---

**END OF ANALYSIS**
