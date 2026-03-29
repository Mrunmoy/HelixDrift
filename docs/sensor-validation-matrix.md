# Sensor Validation Matrix

Quantitative acceptance criteria for each sensor simulator and driver path.
These thresholds define what "sensor independently proven" means for
Milestone 1 of the simulation backlog.

## Purpose

Before three-sensor fusion is trusted, each sensor must independently pass
these checks. Failures at this level must be distinguishable from fusion
failures.

## Test Categories

Each sensor has four categories of validation:

1. **Driver interface** — probe, init, register access
2. **Physical response** — correct output for known physical input
3. **Error injection** — injected errors appear in output as expected
4. **Noise characterization** — statistical properties match configuration

---

## LSM6DSO (IMU: Accelerometer + Gyroscope)

### Driver Interface

| Test | Input | Expected Output | Tolerance |
|------|-------|-----------------|-----------|
| WHO_AM_I | Read reg 0x0F | 0x6C | exact |
| Probe | I2C probe at 0x6A | true | exact |
| Init | Call `imu.init()` | true | exact |
| CTRL1_XL write/read | Write 0x60 to reg 0x10 | Read back 0x60 | exact |
| CTRL2_G write/read | Write 0x60 to reg 0x11 | Read back 0x60 | exact |

### Accelerometer Physical Response

| Test | Gimbal Orientation | Expected Accel (g) | Tolerance (g) |
|------|-------------------|---------------------|----------------|
| Flat (Z up) | Identity | [0, 0, +1.0] | 0.05 per axis |
| Inverted (Z down) | 180 deg around X | [0, 0, -1.0] | 0.05 per axis |
| +X up | 90 deg around Y | [+1.0, 0, 0] | 0.05 per axis |
| -X up | -90 deg around Y | [-1.0, 0, 0] | 0.05 per axis |
| +Y up | -90 deg around X | [0, +1.0, 0] | 0.05 per axis |
| -Y up | 90 deg around X | [0, -1.0, 0] | 0.05 per axis |
| Norm check (any pose) | Any | \|accel\| = 1.0 | 0.02 |

### Gyroscope Physical Response

| Test | Rotation Rate (rad/s) | Expected Gyro (dps) | Tolerance (dps) |
|------|----------------------|---------------------|-----------------|
| Stationary | [0, 0, 0] | [0, 0, 0] | 1.0 per axis |
| +Z rotation 1 rad/s | [0, 0, 1.0] | [0, 0, 57.3] | 3.0 |
| -Z rotation 1 rad/s | [0, 0, -1.0] | [0, 0, -57.3] | 3.0 |
| +X rotation 2 rad/s | [2.0, 0, 0] | [114.6, 0, 0] | 5.0 |
| Multi-axis | [1.0, 0.5, -0.3] | [57.3, 28.6, -17.2] | 3.0 per axis |

### Accelerometer Error Injection

| Test | Injected Error | Measurement | Expected | Tolerance |
|------|---------------|-------------|----------|-----------|
| Bias recovery | bias = [0.02, -0.01, 0.03] g | Mean over 200 samples (stationary) | Matches injected | 0.005 g per axis |
| Scale recovery | scale = [1.02, 0.98, 1.01] | Paired measurements (6 poses) | Recovered within | 0.5% per axis |
| Bias + scale combined | Both injected | 6-position tumble recovery | Both recovered | bias: 0.005 g, scale: 0.5% |

### Gyroscope Error Injection

| Test | Injected Error | Measurement | Expected | Tolerance |
|------|---------------|-------------|----------|-----------|
| Static bias | bias = [0.005, -0.003, 0.008] rad/s | Mean over 500 samples (stationary) | Matches injected | 2 * noise_stddev / sqrt(N) |
| Rate scale | scale = [1.03, 0.97, 1.01] | Paired ±90 dps measurements | Recovered within | 0.5% per axis |
| Integration drift | bias = 0.01 rad/s, 360 deg rotation | Final angle error | Proportional to bias*time | 2 degrees |

### Noise Characterization

| Test | Configuration | Measurement | Expected | Tolerance |
|------|--------------|-------------|----------|-----------|
| Accel noise stddev | noiseStdDev = 0.002 g | Empirical stddev over 500 samples | 0.002 g | 10% |
| Gyro noise stddev | noiseStdDev = 0.001 rad/s | Empirical stddev over 500 samples | 0.001 rad/s | 10% |
| Deterministic seed | Same seed, same config | Two runs produce identical output | Bit-identical | exact |

---

## BMM350 (Magnetometer)

### Driver Interface

| Test | Input | Expected Output | Tolerance |
|------|-------|-----------------|-----------|
| CHIP_ID | Read reg 0x00 | 0x33 | exact |
| Probe | I2C probe at 0x14 | true | exact |
| Init | Call `mag.init()` | true | exact |
| PMU command | Write normal mode cmd | PMU status = normal | exact |
| OTP read | Read OTP word 0 | Returns stored value | exact |
| Soft reset | Write 0xB6 to CMD | Registers return to defaults | exact |

### Physical Response

| Test | Gimbal Orientation | Expected Mag (uT) | Tolerance (uT) |
|------|-------------------|--------------------|-----------------------|
| Identity | Identity | [25, 0, -40] (earth field) | 2.0 per axis |
| 90 deg yaw | 90 deg around Z | [0, -25, -40] | 2.0 per axis |
| 180 deg yaw | 180 deg around Z | [-25, 0, -40] | 2.0 per axis |
| 90 deg pitch | 90 deg around Y | [-40, 0, -25] | 3.0 per axis |
| Field magnitude | Any orientation | \|mag\| = 47.2 | 1.0 |

Note: Default earth field is [25, 0, 40] in the simulator (stored as
horizontal=25, vertical=40). The sign convention depends on whether
Down is positive or negative Z in the sensor frame. Verify against the
actual simulator output before setting thresholds.

### Error Injection

| Test | Injected Error | Measurement | Expected | Tolerance |
|------|---------------|-------------|----------|-----------|
| Hard iron | hardIron = [10, -15, 8] uT | Reading at identity | Baseline + offset | 1.0 uT per axis |
| Hard iron constant | hardIron = [10, -15, 8] uT | Readings at 6 orientations | Offset constant across poses | 1.5 uT per axis |
| Soft iron diagonal | softIronScale = [1.1, 0.9, 1.05] | Field magnitude per axis | Axes scaled proportionally | 2% per axis |
| Sphere fit recovery | hardIron = [10, -15, 8] | 216 samples (3 rotations) | Recovered center = [10, -15, 8] | 1.0 uT (noiseless), 2.0 uT (with noise) |
| Noise injection | noiseStdDev = 0.3 uT | Empirical stddev over 200 samples | 0.3 uT | 15% |

### Noise Characterization

| Test | Configuration | Measurement | Expected | Tolerance |
|------|--------------|-------------|----------|-----------|
| Noise stddev | noiseStdDev = 0.5 uT | Empirical stddev over 500 samples | 0.5 uT | 10% |
| Deterministic seed | Same seed, same config | Two runs identical | Bit-identical | exact |

---

## LPS22DF (Barometric Pressure)

### Driver Interface

| Test | Input | Expected Output | Tolerance |
|------|-------|-----------------|-----------|
| WHO_AM_I | Read reg 0x0F | 0xB4 | exact |
| Probe | I2C probe at 0x5D | true | exact |
| Init | Call `baro.init()` | true | exact |
| CTRL_REG1 write/read | Write then read | Read back matches | exact |
| Soft reset | Write reset bit to CTRL_REG2 | Registers return to defaults | exact |

### Physical Response

| Test | Altitude (m) | Expected Pressure (hPa) | Tolerance (hPa) |
|------|-------------|------------------------|-----------------|
| Sea level | 0 | 1013.25 | 0.5 |
| 100m | 100 | 1001.3 | 1.0 |
| 500m | 500 | 954.6 | 2.0 |
| 1000m | 1000 | 898.7 | 3.0 |
| -100m (below sea level) | -100 | 1025.4 | 1.0 |
| Pressure override | setPressure(950.0) | 950.0 | 0.5 |

### Temperature

| Test | Set Temperature (C) | Expected Raw | Tolerance |
|------|--------------------|--------------|--------------------|
| Room temp | 22.0 | 22.0 C readback | 0.5 C |
| Cold | -10.0 | -10.0 C readback | 0.5 C |
| Hot | 60.0 | 60.0 C readback | 0.5 C |
| Bias | bias = 2.0 C, temp = 22.0 | 24.0 C readback | 0.5 C |

### Error Injection

| Test | Injected Error | Measurement | Expected | Tolerance |
|------|---------------|-------------|----------|-----------|
| Pressure bias | bias = 5.0 hPa | Read at sea level | 1018.25 hPa | 0.5 hPa |
| Pressure noise | noiseStdDev = 0.1 hPa | Empirical stddev over 200 samples | 0.1 hPa | 15% |
| Temp bias | tempBias = 3.0 C | Read temperature | 25.0 C (22 + 3) | 0.5 C |

### Noise Characterization

| Test | Configuration | Measurement | Expected | Tolerance |
|------|--------------|-------------|----------|-----------|
| Pressure noise | noiseStdDev = 0.2 hPa | Empirical stddev over 500 samples | 0.2 hPa | 10% |
| Deterministic seed | Same seed, same config | Two runs identical | Bit-identical | exact |

---

## Cross-Sensor Checks

These tests verify the simulator infrastructure works correctly across sensors.

| Test | Condition | Expected | Tolerance |
|------|-----------|----------|-----------|
| Gimbal sync consistency | Rotate gimbal, sync all sensors | All sensors see same orientation | Same timestep |
| Dual I2C bus isolation | Read from I2C0 and I2C1 | Sensors on different buses work independently | exact |
| Sensor independence | Inject error on IMU only | Mag and baro unaffected | exact |

---

## Acceptance Criteria Summary

A sensor is "independently proven" when all tests in its section pass.

| Sensor | Driver | Physical | Error Injection | Noise | Status |
|--------|--------|----------|-----------------|-------|--------|
| LSM6DSO | 5 tests | 12 tests | 6 tests | 3 tests | pending |
| BMM350 | 6 tests | 5 tests | 5 tests | 2 tests | pending |
| LPS22DF | 5 tests | 6 tests | 3 tests | 2 tests | pending |
| Cross-sensor | — | 3 tests | — | — | pending |

Total: ~56 sensor-level validation tests needed.

Current state: existing tests cover driver interface and basic physical
response. The gaps are primarily in quantitative error injection recovery,
noise characterization, and deterministic seeding validation.
