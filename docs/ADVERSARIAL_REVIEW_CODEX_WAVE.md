# Adversarial Review: Codex Wave 1-3 Implementation

**Date:** 2026-03-29  
**Reviewers:** Kimi (research/review)  
**Scope:** VirtualI2CBus, LSM6DSO/BMM350/LPS22DF Simulators, VirtualGimbal, VirtualSensorAssembly, VirtualMocapNodeHarness, SimMetrics, Per-Sensor Validation Matrix  
**Status:** ✅ Tests passing (207/207), but significant idealizations remain

---

## Executive Summary

Codex delivered robust baseline simulators with deterministic seeding, per-sensor validation matrices, and end-to-end integration harnesses. However, **multiple idealizations exist that will cause real-hardware tests to fail** when the team transitions from simulation to silicon. This document identifies these gaps so they can be addressed in Milestones 4-6.

---

## 1. Idealized Assumptions in Current Simulators

### 1.1 I2C Bus: Idealized Communication

| Aspect | Current Simulation | Real Hardware Reality | Risk Level |
|--------|-------------------|----------------------|------------|
| **Timing** | Instantaneous read/write | SCL stretching, bus contention, rise/fall times | 🔴 High |
| **Errors** | None modeled | NACK, arbitration loss, clock errors | 🔴 High |
| **Multi-master** | Single master assumed | Multi-node bus requires arbitration | 🟡 Medium |
| **Noise** | Perfect signal integrity | Crosstalk, EMI in wearable environment | 🟡 Medium |

**Evidence:** `VirtualI2CBus` calls `device.readRegister()` synchronously with no timing model:
```cpp
// Current: Instantaneous
bool VirtualI2CBus::readRegister(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
    auto it = devices_.find(addr);
    if (it == devices_.end()) return false;
    return it->second.get().readRegister(reg, buf, len);  // No timing
}
```

**Impact:** Code that works in simulation may fail on real I2C with timing-sensitive sensors.

### 1.2 Sensor Initialization: Instant Success

| Aspect | Current Simulation | Real Hardware Reality | Risk Level |
|--------|-------------------|----------------------|------------|
| **Power-on delay** | None | 10-50ms boot time, power sequencing | 🟡 Medium |
| **WHO_AM_I retry** | Single attempt | May require retries with delays | 🟡 Medium |
| **OTP read timing** | Instant | BMM350 OTP requires ~1ms per word | 🟡 Medium |
| **Sensor Ready** | Always ready | Status polling required | 🟡 Medium |

**Evidence:** Tests assume immediate successful init:
```cpp
TEST_F(Lsm6dsoSimulatorTest, Probe_Succeeds) {
    EXPECT_TRUE(bus.probe(IMU_ADDR));  // Instant, always true
}
```

### 1.3 Magnetometer: Perfect World Model

| Aspect | Current Simulation | Real Hardware Reality | Risk Level |
|--------|-------------------|----------------------|------------|
| **Earth field** | Constant 25/40 µT | Varies 25-65 µT by location, ±10% temporal | 🔴 High |
| **Magnetic declination** | Fixed | 0-20° variation by geographic location | 🔴 High |
| **Body interference** | None | Soft iron from batteries, PCBs, motors | 🔴 High |
| **External disturbance** | None | Furniture, electronics, vehicles nearby | 🔴 High |
| **Temperature drift** | None | ±0.1% / °C typical | 🟡 Medium |
| **Calibration** | Perfect OTP data | Real OTP requires compensation | 🟡 Medium |

**Evidence:** BMM350 simulator uses static field:
```cpp
static constexpr float DEFAULT_EARTH_FIELD_HORIZONTAL = 25.0f;
static constexpr float DEFAULT_EARTH_FIELD_VERTICAL   = 40.0f;  // Fixed!
```

**Impact:** AHRS will diverge significantly in real environments. See Section 3 for proposed disturbance modeling.

### 1.4 IMU: Ideal Noise and Bias Model

| Aspect | Current Simulation | Real Hardware Reality | Risk Level |
|--------|-------------------|----------------------|------------|
| **Gyro bias** | Constant or user-set | Random walk, temperature-dependent | 🔴 High |
| **Gyro noise** | White Gaussian | Bias instability, rate random walk | 🔴 High |
| **Accel bias** | Constant | Temperature-dependent, vibration-sensitive | 🟡 Medium |
| **Cross-axis** | Perfect alignment | 0.5-2% cross-axis sensitivity | 🟡 Medium |
| **Non-linearity** | Perfect linear | ±0.5% FS non-linearity | 🟡 Medium |

**Evidence:** LSM6DSO uses simple Gaussian noise:
```cpp
void Lsm6dsoSimulator::setSeed(uint32_t seed) {
    rng_.seed(seed);
}
// ... noise generation only uses rng_ for white noise
```

No bias random walk, no temperature model, no vibration sensitivity.

### 1.5 Barometer: Static Environment

| Aspect | Current Simulation | Real Hardware Reality | Risk Level |
|--------|-------------------|----------------------|------------|
| **Weather changes** | None | ±50 hPa over hours/days | 🟡 Medium |
| **Wind/pressure gradients** | None | Local pressure variations | 🟢 Low |
| **Temperature coupling** | None | Pressure sensor tempco | 🟡 Medium |
| **Response time** | Instant | Low-pass thermal response | 🟡 Medium |

### 1.6 Timing: Perfect Clocks

| Aspect | Current Simulation | Real Hardware Reality | Risk Level |
|--------|-------------------|----------------------|------------|
| **Timestamp accuracy** | Perfect µs | ±1-5% clock drift between nodes | 🔴 High |
| **Sampling regularity** | Perfect interval | Jitter from scheduler, I2C delays | 🟡 Medium |
| **Loop timing** | Deterministic | Real-time constraints, preemption | 🟡 Medium |

**Evidence:** `VirtualClock` advances deterministically:
```cpp
void advanceUs(uint64_t deltaUs) { now += deltaUs; }  // No jitter, no drift
```

---

## 2. Missing Simulator Fidelity (Critical Gaps)

### 2.1 Temperature Effects (All Sensors)

**Missing:** Temperature-dependent bias drift model for IMU and magnetometer.

**Required for Milestone 5 (Calibration):**
- Tempco model: bias(T) = bias(T₀) + TC × (T - T₀)
- Self-heating during operation
- Environmental temperature changes

### 2.2 In-Run Bias Instability (IMU)

**Missing:** Allan variance characteristics of gyro/accel.

**Required for Milestone 4 (Motion Validation):**
- Bias random walk (0.1-1 °/hr^0.5 typical for MEMS)
- Rate random walk
- Correlation time constants

### 2.3 Soft Iron Disturbance Field

**Missing:** Spatially-varying magnetic disturbances.

**Required for Milestone 6 (Environmental Testing):**
- Time-varying disturbance vectors
- Body-proximity effects (ferromagnetic materials near sensor)
- Furniture/electronics simulation

### 2.4 I2C Timing and Contention

**Missing:** Realistic bus timing model.

**Required for Milestone 4 (Protocol Validation):**
- Transaction duration modeling
- Clock stretching simulation
- Multi-node bus arbitration

### 2.5 Packet Loss and Latency (Transport)

**Missing:** Network impairment model.

**Required for Milestone 4 (RF/Sync):**
- Probabilistic packet loss
- Variable latency
- Burst loss patterns

---

## 3. Recommended Additions for Milestones 4-6

### 3.1 Environment Disturbance Model (High Priority)

Create `MagneticEnvironment` class:
```cpp
class MagneticEnvironment {
public:
    void setEarthField(float horizontal, float vertical, float declination);
    void addDisturbance(const Vec3& position, const Vec3& moment);  // Dipole model
    void setTemperature(float tempC);
    Vec3 getFieldAt(const Vec3& position) const;
};
```

### 3.2 IMU Noise with Allan Variance (High Priority)

Extend `Lsm6dsoSimulator` with:
```cpp
struct AllanVarianceParams {
    float biasInstability;     // °/hr at optimal averaging
    float angleRandomWalk;     // °/sqrt(hr)
    float rateRandomWalk;      // °/hr/sqrt(hr)
    float correlationTime;     // seconds
};
```

### 3.3 Temperature Model (Medium Priority)

Add to all sensors:
```cpp
void setTemperature(float tempC);
void setTempco(const Vec3& tempco);  // bias drift per °C
```

### 3.4 Network Impairment (High Priority)

See companion document `RF_SYNC_SIMULATION_SPEC.md`.

### 3.5 Calibration State Tracking (Medium Priority)

Add to `Bmm350Simulator`:
```cpp
enum class CalibrationState {
    UNCALIBRATED,      // Raw OTP only
    HARD_IRON_COMP,    // Hard iron compensated
    SOFT_IRON_COMP,    // Full soft iron matrix
    FIELD_ALIGNED      // Aligned to local mag north
};
```

---

## 4. Validation Matrix Gaps

The current PER_SENSOR_VALIDATION_MATRIX.md covers functional correctness but misses:

| Test Category | Current Coverage | Missing |
|--------------|------------------|---------|
| **Temperature** | ❌ None | Bias drift over [-20, 60]°C |
| **Allan Variance** | ❌ None | Characteristic noise at τ=1-100s |
| **Magnetic Disturbance** | ❌ None | Recovery from field perturbation |
| **I2C Timing** | ❌ None | Timeout handling, retry logic |
| **Packet Loss** | ❌ None | Recovery from dropped frames |

---

## 5. Conclusion

**Current State:** Strong foundation with deterministic, reproducible tests.

**Critical Gaps Before Hardware:**
1. **Magnetic disturbance modeling** - Real environments have time-varying fields
2. **IMU in-run bias stability** - Current noise model insufficient for long-duration tests
3. **Timing/clock drift** - Perfect clocks hide synchronization algorithm defects
4. **I2C realism** - Instant I2C won't catch timing-sensitive driver bugs

**Recommendation:**
- ✅ Proceed with Milestone 4 (RF/Sync) using specs in companion documents
- ⚠️ Address magnetic disturbance modeling before Milestone 6
- ⚠️ Add temperature/Allan variance before calibration validation (Milestone 5)

---

## Appendix: Test Failures Expected on Real Hardware

Without addressing these gaps, expect these test categories to fail:

| Test | Simulation Result | Expected Hardware Result |
|------|------------------|-------------------------|
| `FullRotation360DegreesReturnsToStart` | ✅ Pass | ❌ Fail (mag interference) |
| `MagReadsEarthField` | ✅ Pass | ⚠️ Variable (location-dependent) |
| `GyroReadsRotationRate` | ✅ Pass | ⚠️ Offset (bias drift) |
| `PressureChangesWithAltitude` | ✅ Pass | ❌ Fail (weather-dependent) |

**Mitigation:** Add environmental tolerance bands and disturbance rejection tests before hardware bring-up.
