# Design: ESP32-WROOM-32 Mocap Node

## Status
Draft — pre-implementation

## Hardware

| Item | Detail |
|---|---|
| MCU | ESP32-D0WDQ6 rev1.0, dual-core 240 MHz |
| Flash | 4 MB |
| IMU | MPU-6050 (accel + gyro), I2C addr 0x68 (AD0→GND) |
| Magnetometer | BMM350, I2C addr 0x14 (ADDR→GND) |
| I2C bus | SDA=GPIO32, SCL=GPIO33, 400 kHz |
| USB-Serial | /dev/ttyUSB0 |

### Wiring

```
WROOM-32        MPU-6050        BMM350
─────────       ─────────       ──────
3V3       ───── VCC             VCC
GND       ───── GND             GND
GPIO32    ───── SDA             SDA
GPIO33    ───── SCL             SCL
              AD0 → GND         ADDR → GND
```

Pull-ups: 4.7 kΩ on SDA and SCL to 3V3.

---

## Goals

1. Read MPU-6050 (accel + gyro) and BMM350 (mag) via a single I2C bus.
2. Run Mahony AHRS (9-DOF when mag available, 6-DOF fallback) at 200 Hz.
3. Log quaternion output over serial at 10 Hz for initial validation.
4. Maintain BLE OTA capability from the esp32s3-xiao baseline.
5. All core logic unit-tested on host before flashing.

---

## Architecture

```
┌─────────────────────────────────────────────────┐
│  app_main  (main.c)                             │
│    board_wroom32_init()                         │
│    wroom32_ota_confirm_image()                  │
│    loop: ESP_LOGI "alive" every 1 s             │
└────────────────┬────────────────────────────────┘
                 │ spawns
    ┌────────────┴──────────┐
    │  NimBLE OTA task      │   (BleOtaService, OtaManager,
    │  (existing baseline)  │    Esp32OtaFlashBackend)
    └───────────────────────┘
    ┌───────────────────────┐
    │  sensor_fusion_task   │   FreeRTOS task, priority 5
    │  200 Hz tick loop     │
    │                       │
    │  EspI2CBus            │◄── GPIO32/33, 400 kHz
    │  EspDelayProvider     │
    │  sf::MPU6050          │   accel + gyro
    │  sf::BMM350           │   mag (optional — 6-DOF fallback)
    │  sf::MahonyAHRS       │   quaternion output
    └───────────────────────┘
```

### New components (this feature)

#### `EspI2CBus`  (`src/EspI2CBus.hpp/.cpp`)
- Implements `sf::II2CBus`
- Wraps ESP-IDF `i2c_master` driver (i2c_master_bus_handle_t)
- Constructor takes port number, SDA pin, SCL pin, clock Hz
- `readRegister`, `writeRegister`, `probe` map to ESP-IDF i2c_master transactions
- Single instance, shared by both sensor drivers

#### `EspDelayProvider`  (`src/EspDelayProvider.hpp`)
- Implements `sf::IDelayProvider`
- Header-only
- `delayMs(n)` → `vTaskDelay(pdMS_TO_TICKS(n))`
- `delayUs(n)` → `esp_rom_delay_us(n)`

#### `SensorFusionTask`  (`src/SensorFusionTask.hpp/.cpp`)
- Owns `EspI2CBus`, `EspDelayProvider`, `sf::MPU6050`, `sf::BMM350`, `sf::MahonyAHRS`
- Static `start()` factory that spawns the FreeRTOS task
- Private `run()` implements the 200 Hz tick loop
- Graceful degradation: if BMM350 init fails, runs 6-DOF only (logs warning)

---

## Sensor Fusion Parameters

| Parameter | Value | Rationale |
|---|---|---|
| Loop rate | 200 Hz | MPU-6050 max ODR at DLPF 44 Hz |
| Mag decimation | 4× (50 Hz) | BMM350 ODR 100 Hz, read every other tick |
| Mahony Kp | 1.0 | Standard convergence |
| Mahony Ki | 0.01 | Small integral to remove gyro bias |
| Accel range | ±2 g | Sufficient for slow mocap motion |
| Gyro range | ±500 dps | Covers fast limb rotation |
| DLPF | 44 Hz | Noise rejection, matches 200 Hz ODR |

---

## Partition Table (4 MB flash)

```
nvs,      data, nvs,   0x9000,  0x4000
otadata,  data, ota,   0xd000,  0x2000
phy_init, data, phy,   0xf000,  0x1000
ota_0,    app,  ota_0, 0x10000, 0x1E0000   (1920 KB)
ota_1,    app,  ota_1, 0x1F0000,0x1E0000   (1920 KB)
```

Total: 3 × 1920 KB OTA slots + overhead < 4 MB.

---

## Host Test Coverage

| Test file | What it validates |
|---|---|
| `test_esp_i2c_bus.cpp` | `EspI2CBus` read/write/probe routes through mock ESP-IDF calls |
| `test_sensor_fusion_task.cpp` | Task init, 9-DOF path, 6-DOF fallback when BMM350 absent |

Existing tests (`test_ota_manager.cpp`, etc.) must continue to pass unchanged.

---

## Definition of Done

- [ ] Host tests green (`./build.py --host-only -t`)
- [ ] ESP32 build succeeds (`./build.py --wroom32-only`)
- [ ] Flashes and boots without panic
- [ ] Serial shows quaternion output at ~10 Hz
- [ ] I2C scan confirms 0x68 (MPU-6050) and 0x14 (BMM350) on bus
- [ ] BLE advertising visible ("HelixDrift-OTA")
- [ ] OTA rollback timer cancelled (no revert on reset)
- [ ] Design doc updated with any findings
