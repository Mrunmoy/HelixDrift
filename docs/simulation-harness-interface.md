# SimulationHarness Interface Specification

Design contract for the reusable test harness that orchestrates sensor
simulation, fusion pipeline execution, and result capture.

This document defines the interface. Implementation is owned by Codex.

## Purpose

Eliminate ~50 lines of boilerplate per integration test. Provide
deterministic, reproducible simulation runs with captured time-series
data and computed metrics.

## Ownership

- Interface design: Claude / Systems Architect
- Implementation: Codex / Fusion And Kinematics
- CSV export and plotting: Codex / Host Tools And Evidence

---

## Core Types

### HarnessConfig

```cpp
namespace sim {

struct ImuErrorConfig {
    sf::Vec3 accelBias{0.0f, 0.0f, 0.0f};       // g
    sf::Vec3 gyroBias{0.0f, 0.0f, 0.0f};        // rad/s
    sf::Vec3 accelScale{1.0f, 1.0f, 1.0f};      // multiplicative
    sf::Vec3 gyroScale{1.0f, 1.0f, 1.0f};       // multiplicative
    float accelNoiseStdDev = 0.0f;               // g
    float gyroNoiseStdDev = 0.0f;                // rad/s
};

struct HarnessConfig {
    // Sensor error injection
    ImuErrorConfig imuErrors{};
    Bmm350Simulator::ErrorConfig magErrors{};
    float baroNoise = 0.0f;       // hPa
    float baroBias = 0.0f;        // hPa

    // Pipeline tuning
    float mahonyKp = 1.0f;
    float mahonyKi = 0.0f;
    float dtSeconds = 0.02f;      // 50 Hz default
    bool preferMag = true;

    // Environment
    sf::Vec3 earthField{25.0f, 0.0f, -40.0f};  // uT

    // Reproducibility
    uint32_t rngSeed = 42;
};

} // namespace sim
```

**Design rules:**
- `rngSeed` is passed to all three simulators via `setSeed()`. Two runs
  with the same config must produce bit-identical results.
- `ImuErrorConfig` maps to the individual setter calls on
  `Lsm6dsoSimulator` (there is no `ErrorConfig` struct on that class).
- `magErrors` uses `Bmm350Simulator::ErrorConfig` directly.

### TimeSeriesSample

```cpp
namespace sim {

struct TimeSeriesSample {
    float timeS;                     // simulation time
    sf::Quaternion truthQuat;        // from VirtualGimbal
    sf::Quaternion fusedQuat;        // from MocapNodePipeline
    sf::Vec3 rawAccel;               // g (from driver readout)
    sf::Vec3 rawGyro;                // dps (from driver readout)
    sf::Vec3 rawMag;                 // uT (from driver readout)
    float pressureHPa;               // from driver readout
    float angularErrorDeg;           // pre-computed geodesic error
};

} // namespace sim
```

### RunResult

```cpp
namespace sim {

struct RunResult {
    std::vector<TimeSeriesSample> timeSeries;

    // Summary metrics (computed from timeSeries)
    float rmsErrorDeg;              // sqrt(mean(error^2)) over full run
    float maxErrorDeg;              // max(error) over full run
    float meanErrorDeg;             // mean(error) over full run
    float driftRateDegPerMin;       // linear fit on error over last 50%
    float convergenceTimeS;         // time until error stays < 5 degrees
    bool completed;                 // false if pipeline.step() failed
};

} // namespace sim
```

**Metric computation rules:**
- `rmsErrorDeg`: over all samples
- `driftRateDegPerMin`: linear regression on `angularErrorDeg` vs `timeS`
  using only the last 50% of samples (skip transient convergence)
- `convergenceTimeS`: earliest time `t` such that `angularErrorDeg < 5.0`
  for all subsequent samples. If never converges, set to `duration + 1`.

---

## Angular Error Function

```cpp
namespace sim {

float angularErrorDeg(const sf::Quaternion& truth,
                      const sf::Quaternion& fused);

} // namespace sim
```

**Specification:**
- `dot = |truth.w*fused.w + truth.x*fused.x + truth.y*fused.y + truth.z*fused.z|`
- `dot = clamp(dot, 0.0f, 1.0f)`
- `return 2.0f * acos(dot) * 180.0f / pi`
- Handles double-cover: `q` and `-q` return 0 error
- Never returns NaN or negative values

**Required tests:**
- `angularErrorDeg(identity, identity)` = 0
- `angularErrorDeg(q, -q)` = 0
- `angularErrorDeg(identity, 90deg_around_Z)` ~= 90
- `angularErrorDeg(identity, 180deg_around_Z)` ~= 180
- No NaN for any finite input

---

## SimulationHarness Class

```cpp
namespace sim {

class SimulationHarness {
public:
    explicit SimulationHarness(const HarnessConfig& cfg = {});

    // Access internals for custom test logic
    VirtualGimbal& gimbal();
    sf::MocapNodePipeline& pipeline();

    // Single step: advance gimbal by dt, sync sensors, step pipeline
    TimeSeriesSample step();

    // Run for a fixed duration
    RunResult runForDuration(float seconds);

    // Run a JSON motion script
    RunResult runMotionScript(const std::string& jsonPath);
};

} // namespace sim
```

### Constructor Behavior

1. Create two `VirtualI2CBus` instances (I2C0 and I2C1)
2. Create three sensor simulators
3. Register: LSM6DSO at 0x6A on I2C0, BMM350 at 0x14 on I2C1,
   LPS22DF at 0x5D on I2C1
4. Create and configure `VirtualGimbal`, attach all three sensors
5. Call `setSeed(cfg.rngSeed)` on all three simulators
6. Apply error configs: call individual setters on LSM6DSO, call
   `setErrors()` on BMM350, call `setPressureBias/NoiseStdDev` on
   LPS22DF
7. Set `gimbal.setEarthField(cfg.earthField)`
8. Create `MockDelay` (no-op, same as existing test fixture)
9. Create real sensor drivers (LSM6DSO, BMM350, LPS22DF) using the
   virtual I2C buses
10. Call `init()` on all three drivers
11. Create `MocapNodePipeline` with the configured Kp/Ki/dt/preferMag
12. Reset gimbal to identity, sync to sensors

### step() Behavior

1. Advance internal time by `cfg.dtSeconds`
2. Call `gimbal.update(dt)` (applies current rotation rate)
3. Call `gimbal.syncToSensors()`
4. Call `pipeline.step(sample)`
5. Read raw sensor data from drivers (accel, gyro, mag, pressure)
6. Compute `angularErrorDeg(gimbal.getOrientation(), sample.orientation)`
7. Return populated `TimeSeriesSample`

### runForDuration(float seconds) Behavior

1. Compute `N = seconds / cfg.dtSeconds`
2. Call `step()` N times, accumulate into `timeSeries`
3. Compute summary metrics
4. Return `RunResult`

### runMotionScript(string path) Behavior

1. Load JSON motion script into gimbal
2. Step through time at `dtSeconds` intervals, applying motion script
   actions at the appropriate simulation times
3. Accumulate samples and compute metrics
4. Return `RunResult`

---

## CSV Export

```cpp
namespace sim {

bool shouldExport();  // checks HELIX_TEST_EXPORT=1 env var
void exportCsv(const RunResult& result, const std::string& path);

} // namespace sim
```

### CSV Schema

```
time_s,truth_w,truth_x,truth_y,truth_z,fused_w,fused_x,fused_y,fused_z,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,mag_x,mag_y,mag_z,pressure_hpa,error_deg
```

- One header row
- One data row per `TimeSeriesSample`
- 6 decimal places for all floating-point values
- Empty `RunResult` produces header-only file
- Output path: `${CMAKE_BINARY_DIR}/test_output/<test_name>.csv`
- Only writes when `HELIX_TEST_EXPORT=1` environment variable is set

---

## File Layout

```
simulators/
  harness/
    SimTypes.hpp          # TimeSeriesSample, RunResult, HarnessConfig,
                          # ImuErrorConfig structs
    SimMetrics.hpp        # angularErrorDeg() declaration
    SimMetrics.cpp        # angularErrorDeg() implementation
    SimulationHarness.hpp # SimulationHarness class
    SimulationHarness.cpp # SimulationHarness implementation
    CsvExport.hpp         # shouldExport(), exportCsv() declarations
    CsvExport.cpp         # CSV export implementation
  tests/
    test_sim_metrics.cpp          # angularErrorDeg unit tests
    test_simulation_harness.cpp   # harness integration tests
```

### CMake Integration

- Add `simulators/harness/*.cpp` to `helix_simulators` library
- Add `simulators/harness/` to include paths
- Add new test files to `helix_integration_tests`
- Add `test_output/` to `.gitignore`

---

## Implementation Priorities

1. `SimTypes.hpp` + `SimMetrics.hpp/.cpp` + tests — no dependencies
2. `CsvExport.hpp/.cpp` — depends on SimTypes
3. `SimulationHarness.hpp/.cpp` — depends on SimTypes + SimMetrics
4. `test_simulation_harness.cpp` — depends on everything above
5. CMake wiring — after all files exist

---

## Constraints

- The existing `SensorFusionIntegrationTest` fixture must not be modified.
  It remains the simple, readable smoke test. The harness is a new tier.
- The harness uses STL containers (`std::vector`) — this is test/host code,
  not embedded firmware. This is explicitly allowed by the coding standards.
- No dynamic allocation in the metric computation functions themselves; they
  operate on pre-allocated vectors.
- The harness must be usable from `TEST()` macros without requiring a
  fixture base class. Tests that need custom gimbal manipulation between
  steps should use `step()` directly.
