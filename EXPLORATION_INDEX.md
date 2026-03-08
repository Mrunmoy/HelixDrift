# HelixDrift nRF52840 Branch - Exploration Index

**Generated**: 2024-03-08  
**Branch**: `nrf-xiao-nrf52840` (commit 2bd049a)  
**Target**: Seeed XIAO nRF52840  
**Purpose**: MCUBoot OTA Bootloader Implementation Planning

---

## 📋 Report Files

1. **QUICK_SUMMARY.txt** (8 KB)
   - Visual overview with ✅/❌ status indicators
   - Build system, config files, hardware layer summary
   - MCUBoot OTA implementation requirements
   - Key insights and recommended path
   - **START HERE** for high-level understanding

2. **EXPLORATION_REPORT.md** (23 KB)
   - Comprehensive 12-section analysis
   - Full directory tree
   - Detailed build configuration review
   - Application source code walkthrough
   - Version information
   - MCUBoot planning guide
   - **READ THIS** for complete technical details

---

## 🎯 Quick Facts

| Aspect | Status |
|--------|--------|
| **Build System** | Bare CMake + arm-none-eabi-gcc (NO Zephyr) |
| **Target MCU** | nRF52840 (Cortex-M4 @ 64 MHz) |
| **App Type** | Motion Capture Node (sensor fusion + BLE) |
| **App Size** | ~52 KB ELF |
| **Language** | C++17 (no exceptions, no RTTI) |
| **SDK** | Partial nRFx (stubs for off-target testing) |
| **Bootloader** | ❌ NOT IMPLEMENTED |
| **Partition Map** | ❌ NOT DEFINED |
| **OTA Code** | ❌ NOT PRESENT |
| **CI/CD** | ✅ GitHub Actions (host + nRF matrix) |
| **SensorFusion** | v1.1.0 (git submodule) |

---

## 🔍 Key Files to Review

### Build Configuration
- **Root**: `CMakeLists.txt` - Main project config
- **Toolchain**: `tools/toolchains/arm-none-eabi-gcc.cmake` - ARM cross-compiler
- **Dev Env**: `flake.nix` - Nix shell with dependencies
- **Orchestrator**: `build.py` - Python build runner

### Hardware Layer
- **Platform**: `external/SensorFusion/platform/nrf52/` - nRF SDK abstraction
  - `NrfTwimBus.{cpp,hpp}` - I2C driver (TWIM0 + TWIM1)
  - `NrfDelay.{cpp,hpp}` - Microsecond timing
  - `NrfGpio.{cpp,hpp}` - GPIO control
  - `NrfFdsStore.{cpp,hpp}` - Flash storage
- **Board Integration**: `examples/nrf52-mocap-node/src/board_xiao_nrf52840.{cpp,hpp}`
  - Pin definitions
  - Weak symbol hooks for BLE/OTA

### Application
- **Primary App**: `examples/nrf52-mocap-node/src/main.cpp` (motion capture)
- **Demo App**: `examples/nrf52-blinky/src/main.cpp` (GPIO blink)
- **Common Lib**: `firmware/common/` - Shared embedded libraries
  - `BlinkEngine.{cpp,hpp}` - LED timing
  - `MocapBleSender.{cpp,hpp}` - BLE transport interface
  - `MocapHealthTelemetry.hpp` - Battery/health frames
  - `MocapNodeLoop.hpp` - Main loop template

### CI/CD & Documentation
- **CI**: `.github/workflows/ci.yml` - GitHub Actions gate + smoke matrix
- **README**: `README.md` - Quick start, wiring, flashing instructions
- **Tasks**: `TASKS.md` - Execution backlog
- **Validation**: `docs/validation/ON_TARGET_VALIDATION.md` - Hardware checklist

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│ HELIXDRIFT APPLICATION (nrf52_mocap_node)               │
│ ~52 KB ELF, static-linked, bare-metal                   │
└──────────────────────┬──────────────────────────────────┘
                       │
        ┌──────────────┼──────────────┐
        │              │              │
   WEAK SYMBOLS    I2C PLATFORM   SensorFusion
   (for BLE/OTA)   LAYER          LIBRARY (v1.1.0)
        │              │              │
        ├─► xiao_ble_stack_notify()   │
        ├─► xiao_mocap_calibration_command()
        ├─► xiao_mocap_sync_anchor()  │
        ├─► xiao_mocap_health_sample()
        │              │              │
        └──────────────┴──────────────┴─────────────────────┐
                                                            │
     ┌──────────────────────────────────────────────────────┘
     │
     ▼
┌────────────────────────────────────────┐
│ SENSORFUSION PLATFORM (nrf52)          │
│ • NrfTwimBus (I2C)                     │
│ • NrfGpio (GPIO)                       │
│ • NrfDelay (timing)                    │
│ • NrfFdsStore (flash storage)          │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ SEEED XIAO nRF52840 HARDWARE           │
│ • nRF52840 MCU (Cortex-M4 @ 64 MHz)    │
│ • 256 KB RAM, 1 MB Flash               │
│ • Dual I2C (TWIM0, TWIM1)              │
│ • GPIO, ADC, BLE 5.0                   │
└──────────────┬─────────────────────────┘
               │
     ┌─────────┼─────────┐
     │         │         │
     ▼         ▼         ▼
  LSM6DSO   BMM350    LPS22DF
  (accel+    (mag)     (baro)
   gyro)
```

---

## 📊 Build Process

```
User: ./build.py -t
  │
  ├─► build_host()
  │   └─► cmake -S . -B build/host -DHELIXDRIFT_BUILD_TESTS=ON
  │       cmake --build build/host
  │       ctest --test-dir build/host  ← host unit tests (gtest)
  │
  └─► build_nrf()
      └─► git submodule update --init external/SensorFusion
          cmake -S . -B build/nrf -DCMAKE_TOOLCHAIN_FILE=tools/toolchains/arm-none-eabi-gcc.cmake
          cmake --build build/nrf --parallel
          └─► Output: nrf52_blinky, nrf52_mocap_node (ELFs)
```

**CI Pipeline** (GitHub Actions):
1. Gate job: Runs `./build.py -t` on every push/PR
2. Smoke matrix: Parallel `host` and `nrf` builds
3. Artifact upload: ELF + HEX files for release

---

## 🔌 Sensor Configuration

**XIAO Pin Mapping** (from `board_xiao_nrf52840.cpp`):

```
┌─────────────────────────────────────┐
│ SEEED XIAO nRF52840                 │
├─────────────────────────────────────┤
│ TWIM0 (LSM6DSO IMU)                 │
│   D4 (P0.04) → SDA                  │
│   D5 (P0.05) → SCL                  │
│   Frequency: 400 kHz                │
│                                     │
│ TWIM1 (BMM350 + LPS22DF)            │
│   D6 (P1.11) → SDA (shared)         │
│   D7 (P1.12) → SCL (shared)         │
│   Frequency: 400 kHz                │
│                                     │
│ GPIO                                │
│   Pin 13 → LED (blinky demo)        │
│                                     │
│ Power                               │
│   3V3 → all sensors                 │
│   GND → all sensors                 │
└─────────────────────────────────────┘
```

**Sensor Stack** (from `examples/nrf52-mocap-node/src/main.cpp`):

1. **IMU (LSM6DSO)** on TWIM0
   - Accelerometer: 208 Hz (PERFORMANCE mode)
   - Gyroscope: 208 Hz

2. **Magnetometer (BMM350)** on TWIM1
   - Rate: 100 Hz (PERFORMANCE mode)

3. **Barometer (LPS22DF)** on TWIM1
   - Rate: 200 Hz (PERFORMANCE mode)

**Output**: Quaternion frames @ 50 Hz via BLE

---

## 🎮 Application Behavior

### Main Loop (non-blocking)
```cpp
while (true) {
    uint64_t nowUs = clock.nowUs();
    
    // Emit health telemetry every 1 second
    if (nowUs >= nextHealthTickUs) {
        nextHealthTickUs = nowUs + 1000000ULL;
        // Call weak symbol: sf_mocap_health_sample()
        // Emit NODE_HEALTH frame via BLE
    }
    
    // Process one mocap sample (if data available)
    if (!loop.tick()) {
        delay.delayMs(1);  // Sleep if no data
    }
}
```

### Data Flow
```
Raw Sensors (IMU/Mag/Baro)
    ↓
MocapNodePipeline (raw data fusion)
    ↓
Calibration Flow (stationary/T-pose)
    ↓
TimestampSync (central time anchor)
    ↓
Quaternion Output
    ↓
BLE Transport (via weak symbol: xiao_ble_stack_notify)
```

### Calibration Commands (command interface)
- `0`: None
- `1`: Capture stationary
- `2`: Capture T-pose
- `3`: Reset calibration

### Health Telemetry (1 Hz output)
- Battery voltage (mV)
- Battery percentage
- Link quality
- Dropped frame count
- Calibration state
- Flags (reserved)

---

## ⚠️ MCUBoot OTA Considerations

### Current Gaps
- ❌ No bootloader partition
- ❌ No flash partition layout (pm_static.yml)
- ❌ No image trailer or version tracking
- ❌ No firmware signing/verification
- ❌ No rollback protection
- ❌ No OTA update manager

### Integration Points (Already Available)
- ✅ `xiao_ble_stack_notify(data, len)` → Perfect for OTA packets
- ✅ `xiao_mocap_health_sample()` → Could report OTA progress
- ✅ `xiao_mocap_calibration_command()` → Could extend for OTA control
- ✅ Weak symbol pattern → Decouples BLE stack from app

### Recommended Flash Layout (1 MB)
```
0x00000000 - 0x0000FFFF: MCUBoot bootloader (64 KB)
0x00010000 - 0x0007FFFF: App Slot 1 / Primary (448 KB)
0x00080000 - 0x000EFFFF: App Slot 2 / Secondary (448 KB)
0x000F0000 - 0x000FFFFF: NVS/Config (64 KB)
```

### Implementation Steps
1. Create MCUBoot bootloader build target
2. Define linker script for app @ 0x10000
3. Extend `board_xiao_nrf52840.cpp` with OTA manager
4. Implement OTA protocol layer (over BLE notify hook)
5. Add image signing to CI pipeline
6. Test rollback/recovery scenarios

---

## 📚 Documentation

| Document | Purpose | Status |
|----------|---------|--------|
| README.md | Quick start, wiring, flashing | ✅ Complete |
| TASKS.md | Execution backlog | ✅ 90% done |
| ON_TARGET_VALIDATION.md | Hardware validation checklist | ✅ Complete |
| docs/validation/TEST_LOG_TEMPLATE.md | Test result logging | ✅ Ready |
| external/SensorFusion/README.md | Library docs | ✅ Complete |
| datasheets/INDEX.md | Sensor/board datasheets | ✅ Indexed |

---

## 🚀 Getting Started

### Build & Test
```bash
cd /mnt/data/sandbox/embedded/HelixDrift
nix develop  # Enter dev environment
./build.py -t  # Build all + run host tests
```

### Review Key Files
1. Start with QUICK_SUMMARY.txt (this directory)
2. Read main README.md
3. Review examples/nrf52-mocap-node/src/main.cpp
4. Check external/SensorFusion/platform/nrf52/ structure
5. Study board_xiao_nrf52840.cpp for integration points

### Plan MCUBoot Integration
1. Reference QUICK_SUMMARY.txt section "MCUBoot OTA Implementation - WHAT'S NEEDED"
2. Review EXPLORATION_REPORT.md section "SUMMARY FOR MCUBoot OTA PLANNING"
3. Start with linker script + partition layout
4. Implement OTA manager as extension to board layer
5. Integrate with CI/CD for signing

---

## 🔗 External References

- **nRF52840 Datasheet**: See datasheets/INDEX.md
- **Seeed XIAO nRF52840 Wiki**: https://wiki.seeedstudio.com/XIAO_BLE/
- **MCUBoot Docs**: https://docs.mcuboot.com/
- **Nordic nRFx SDK**: https://github.com/NordicSemiconductor/nrfx
- **SensorFusion GitHub**: https://github.com/Mrunmoy/SensorFusion
- **ARM Cortex-M4 ISA**: https://developer.arm.com/documentation/

---

## 📞 Summary

This repository is a **production-ready motion capture node** using sensor fusion and BLE on the Seeed XIAO nRF52840. It uses **bare CMake cross-compilation** (no Zephyr/nRF SDK) with a clean **weak symbol integration** pattern for BLE and OTA extensibility.

**For MCUBoot OTA implementation**, you have solid architectural foundations:
- Clean platform layer (`external/SensorFusion/platform/nrf52/`)
- Proven dual I2C + sensor stack
- Weak symbol hooks for BLE/control
- CI/CD pipeline ready for signing
- No RTOS complexity to work around

**Main task ahead**: Define partition layout, implement bootloader, extend board layer with OTA manager, and wire up BLE OTA protocol.

---

**Generated**: 2024-03-08  
**For questions**: See EXPLORATION_REPORT.md (comprehensive) or QUICK_SUMMARY.txt (overview)
