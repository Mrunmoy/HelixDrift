# Sensor Fusion & Calibration Simulation Test Plan

Brainstorm output from expert panel: IMU calibration, magnetometer calibration, sensor fusion validation, and test architecture.

**Goal:** Validate sensor fusion algorithms and calibration procedures entirely in software using the VirtualGimbal + sensor simulators, before hardware is available. Capture data for graphing and visual inspection.

---

## Table of Contents

1. [Architecture: SimulationHarness](#1-architecture-simulationharness)
2. [Accelerometer Calibration](#2-accelerometer-calibration)
3. [Gyroscope Calibration](#3-gyroscope-calibration)
4. [Magnetometer Calibration](#4-magnetometer-calibration)
5. [Mahony Filter Tuning](#5-mahony-filter-tuning)
6. [Sensor Fusion Validation](#6-sensor-fusion-validation)
7. [Motion Profile Library](#7-motion-profile-library)
8. [Data Export & Visualization](#8-data-export--visualization)
9. [Metrics & Pass/Fail Criteria](#9-metrics--passfail-criteria)
10. [Implementation Phases](#10-implementation-phases)
11. [Simulator Enhancements Needed](#11-simulator-enhancements-needed)

---

## 1. Architecture: SimulationHarness

A reusable class that eliminates the ~50 lines of boilerplate per test. Owns the full stack: gimbal, simulators, I2C buses, real drivers, pipeline, and calibration flow.

```cpp
namespace sim {

struct HarnessConfig {
    // Sensor error injection
    Lsm6dsoSimulator::ErrorConfig imuErrors{};
    Bmm350Simulator::ErrorConfig magErrors{};
    float baroNoise = 0.0f;
    float baroBias = 0.0f;

    // Pipeline tuning
    float mahonyKp = 1.0f;
    float mahonyKi = 0.0f;

    // Simulation
    float dtSeconds = 0.02f;     // 50 Hz
    Vec3 earthField = {25, 0, -40};  // uT
    uint32_t rngSeed = 42;      // deterministic noise
};

struct TimeSeriesSample {
    float timeS;
    Quaternion truthQuat;    // from gimbal
    Quaternion fusedQuat;    // from pipeline
    Vec3 rawAccel, rawGyro, rawMag;
    float pressureHPa;
    float angularErrorDeg;   // pre-computed
};

struct RunResult {
    std::vector<TimeSeriesSample> timeSeries;
    float rmsErrorDeg;
    float maxErrorDeg;
    float driftRateDegPerMin;
    float convergenceTimeS;  // time to < threshold
};

class SimulationHarness {
public:
    explicit SimulationHarness(const HarnessConfig& cfg = {});

    VirtualGimbal& gimbal();
    MocapNodePipeline& pipeline();

    RunResult runMotionScript(const std::string& jsonPath);
    RunResult runForDuration(float seconds);
    TimeSeriesSample step();  // single step for custom loops

    void exportCsv(const RunResult& result, const std::string& path);
};

} // namespace sim
```

**Key design decisions:**
- **Deterministic seeding** (`rngSeed = 42`): Makes noise reproducible. Tests are never flaky.
- **Keep existing tests as-is.** The `SensorFusionIntegrationTest` fixture stays simple and readable. The harness is for the *next tier* of parameterized/export tests.
- **Angular error metric:** `2 * acos(|dot(q_truth, q_fused)|) * 180/pi` -- geodesic distance on quaternion sphere, handles double-cover.

---

## 2. Accelerometer Calibration

### 2a. Six-Position Tumble Test [HIGH]

The gold standard for accel calibration. Place sensor in 6 orientations aligning each axis with gravity in both directions.

**Gimbal orientations:**
| Pose | Quaternion (axis-angle) | Expected accel |
|------|------------------------|----------------|
| +Z up | identity | [0, 0, +1g] |
| -Z up | 180 deg around X | [0, 0, -1g] |
| +X up | 90 deg around Y | [+1g, 0, 0] |
| -X up | -90 deg around Y | [-1g, 0, 0] |
| +Y up | -90 deg around X | [0, +1g, 0] |
| -Y up | 90 deg around X | [0, -1g, 0] |

At each position: collect 200 samples (4 seconds at 50 Hz) while stationary.

**Calibration math:**
```
bias_i = (m_plus_i + m_minus_i) / 2
scale_i = (m_plus_i - m_minus_i) / (2 * 1g)
```

**Inject:** bias `{0.02, -0.01, 0.03}` g, scale `{1.02, 0.98, 1.01}`
**Validate:** Recovered bias/scale match injected within noise limits.

**Graphs:**
- Bar chart: recovered bias vs. injected bias per axis
- Scatter: accel norm before/after calibration across all 6 poses

### 2b. Norm Consistency Check [MEDIUM]

Rotate through 36+ arbitrary orientations. At each, verify `|accel| ~= 1g`. After calibration, the norm spread should tighten.

**Graph:** Histogram of accel magnitude across orientations, before/after cal overlay.

### 2c. Noise Floor Measurement [MEDIUM]

Stationary for 10+ seconds. Compute per-axis std dev. Should match `setAccelNoiseStdDev()` within 10%.

---

## 3. Gyroscope Calibration

### 3a. Static Bias Estimation [HIGH]

Hold perfectly stationary for 10 seconds. Mean of each gyro axis = bias estimate. This is what real devices do at power-up.

**Inject:** bias `{0.005, -0.003, 0.008}` rad/s, noise `0.001` rad/s
**Validate:** `|recovered - injected| < 2 * noise_stddev / sqrt(N)`

**Graph:** Gyro time series per axis with horizontal line at computed mean vs. true bias.

### 3b. Known Rotation Rate Scale Calibration [HIGH]

Rotate at precisely known rates around each axis: +90 deg/s then -90 deg/s.

**Math:**
```
bias_i = (measured_plus + measured_minus) / 2
scale_i = (measured_plus - measured_minus) / (2 * true_rate)
```

**Validate:** Scale recovery within 0.5%. Cross-validate bias with 3a.

**Graph:** Measured rate vs. commanded rate -- should be a line through origin.

### 3c. Integration Drift Test [MEDIUM]

Rotate at 36 deg/s around Z for 10 seconds = 360 degrees. Numerically integrate gyro. Compare final angle to expected 360.

- Without calibration: error proportional to `bias * time + scale_error * 360`
- With calibration: error < 2 degrees

**Graph:** Integrated angle vs. time, with ground truth line. Before/after calibration.

### 3d. Allan Variance (Future Infrastructure) [LOW]

60+ seconds stationary, compute Allan deviation at various tau. For current white-noise-only model, slope should be -0.5 on log-log plot. Becomes useful when random walk / bias instability is added later.

---

## 4. Magnetometer Calibration

### 4a. Hard Iron Sphere Fit [HIGH]

The most common real-world mag error. Hard iron = constant offset that shifts the measurement sphere off-center.

**Minimum test (6 cardinal orientations):**
Collect mag vector at 6 poses. Bounding box center = hard iron estimate.

**Full test (3 orthogonal rotations):**
3 full 360-degree rotations (around X, Y, Z), sampling every 5 degrees = ~216 samples. Least-squares sphere fit: minimize `sum(|p_i - center|^2 - r^2)^2`.

**Inject:** hardIron `{10, -15, 8}` uT, noise `0.3` uT
**Validate:** Recovered center within 0.5 uT (noiseless) or 1.0 uT (with noise). Recovered radius within 1 uT of earth field magnitude (~47.2 uT).

**Graphs:**
- 3D scatter of raw mag readings (offset sphere)
- 3D scatter after correction (centered sphere)
- Histogram of residual magnitudes

### 4b. Soft Iron Ellipsoid Fit [HIGH]

Soft iron distorts the sphere into an ellipsoid (axis-dependent scaling).

**Same gimbal motion as 4a.** After hard iron removal, fit ellipsoid semi-axes.

**Inject:** softIronScale `{1.1, 0.9, 1.05}`, hardIron `{5, -10, 3}` uT
**Validate:** Semi-axis ratios match injected scale within 2%.

**Graphs:**
- 3D scatter: raw (offset ellipsoid) -> hard iron removed (centered ellipsoid) -> fully corrected (sphere)

**Note:** Current simulator only supports diagonal soft iron (per-axis scaling). A full 3x3 matrix would be needed for off-axis ellipsoids -- noted as a future enhancement.

### 4c. Tilt-Compensated Heading [HIGH]

The whole point of accel + mag together.

**Test matrix:** 8 yaw angles x 6 pitch angles x 3 roll angles = 144 poses.

**Tilt compensation math:**
```
mx_h = mx*cos(pitch) + mz*sin(pitch)
my_h = mx*sin(roll)*sin(pitch) + my*cos(roll) - mz*sin(roll)*cos(pitch)
heading = atan2(-my_h, mx_h)
```

**Validate:**
- Zero tilt: heading error < 1 degree
- Up to 45 degrees tilt: < 2 degrees (noiseless), < 5 degrees (with noise)
- Near 80+ degrees tilt: error degrades gracefully, no NaN

**Graph:** Heading error vs. tilt angle (smooth curve, increasing near 90 degrees).

### 4d. Figure-8 Tumble Calibration [MEDIUM]

The user-facing calibration procedure. Model as multi-axis tumble over 12 seconds.

**Quality scoring:**
1. Sphere coverage -- what fraction of the unit sphere is sampled? Good = >60%
2. Fit residual RMS -- after correction, `||corrected_i| - r| < 1 uT`
3. Condition number of design matrix -- high = poorly constrained

**Negative tests (degenerate motions):**
- Single-axis rotation only -> should report low quality
- Very short tumble (<2s) -> not enough samples
- Stationary -> should fail outright

### 4e. Magnetic Interference Rejection [MEDIUM]

**Transient spike:** Add 200 uT hard iron for 0.5 seconds mid-test. Validate:
- With rejection: heading deviation < 10 degrees during spike, recovery < 2 seconds
- Without rejection: heading spikes badly (the baseline)

**Magnitude-based rejection:** If `|mag|` deviates >50% from expected earth field, reduce mag weight in Mahony filter.

**Note:** This requires modifying the Mahony filter to support mag rejection -- a firmware change.

---

## 5. Mahony Filter Tuning

### 5a. Convergence From Initial Error [HIGH]

Start AHRS at wrong orientation (e.g., gimbal at 90 degrees, then snap to identity). Measure convergence.

**Practical approach (no production code changes):** Run 50 steps with gimbal at orientation A, then snap gimbal to identity. AHRS is now ~90 degrees off. Measure recovery.

**Sweep matrix:**
| Kp | Ki | Expected |
|----|----|----------|
| 0.5 | 0.0 | Slow, no overshoot |
| 1.0 | 0.0 | Moderate |
| 5.0 | 0.0 | Fast, possible oscillation |
| 1.0 | 0.05 | Moderate, eliminates steady-state bias |

**Metrics:** T5 (time to <5 deg), T1 (time to <1 deg), overshoot, steady-state error.

**Graph:** Angular error vs. time for each Kp/Ki combination, overlaid.

### 5b. Steady-State During Constant Rotation [HIGH]

30 deg/s around Z, 2-second warmup, then 10 seconds of measurement.

Tests the fundamental Kp vs. dynamic tracking tradeoff -- high Kp causes accel reference to fight gyro during rotation.

**Graph:** Angular error vs. time, with rotation rate on secondary axis.

### 5c. Gyro Bias Rejection via Ki [MEDIUM]

Inject constant gyro bias. Compare Ki=0 (steady drift) vs. Ki=0.05 (integral compensates).

This is the specific reason Ki exists in Mahony -- the integral feedback estimates and cancels gyro bias.

**Graph:** Yaw error vs. time for Ki=0 vs Ki=0.05 vs Ki=0.1. The key deliverable plot.

### 5d. Step Response [MEDIUM]

Stationary -> sudden constant rotation. Measures transient tracking error.

### 5e. Noise Sensitivity Sweep [LOW]

Fix Kp/Ki, sweep accel/gyro noise levels. Measure steady-state jitter. Identifies noise level where filter becomes unreliable.

---

## 6. Sensor Fusion Validation

### 6a. Degraded Sensor Tests [MEDIUM]

| Scenario | Method | Expected |
|----------|--------|----------|
| Mag dropout | Inject 200 uT hard iron mid-run | Falls back to 6DOF, yaw drifts, recovers when mag returns |
| High accel noise (vibration) | accelNoise = 0.5g | Gyro-dominant, larger error but no divergence |
| Large gyro bias (uncalibrated) | 0.5 rad/s bias | Ki=0: large drift. Ki=0.1: compensates in 10-20s |
| All errors simultaneously | Moderate everything | The "realistic worst case" RMS error |

### 6b. Gravity Removal (LinearAccelExtractor) [HIGH]

**Stationary zero test:** Hold at various tilts, extract linear accel, should be ~zero.

**During rotation:** Filter orientation error creates false linear acceleration proportional to `sin(error) * g`. Test this relationship.

**Prerequisite for full test:** `Lsm6dsoSimulator::setLinearAcceleration(Vec3)` to inject known translational acceleration on top of gravity. Currently not supported.

### 6c. Long-Duration Tests [MEDIUM]

60-second stationary run, measure drift rate in deg/min at various error/gain combos. This is the number that tells you real-world performance.

---

## 7. Motion Profile Library

```
simulators/motion_profiles/
    stationary/
        flat_60s.json              # Z-up, no motion, 60s
        tilted_30deg_60s.json      # Pitched 30 degrees, stationary
    calibration/
        six_position_tumble.json   # 6 orientations, 4s each
        gyro_static_10s.json       # Stationary 10 seconds
        gyro_rate_sweep.json       # +/- rate on each axis
        figure_eight_tumble.json   # Multi-axis tumble for mag cal
        stationary_capture.json    # 3s still (for ZUPT)
        tpose_sequence.json        # Still -> T-pose -> still
    single_axis/
        slow_yaw_360.json          # 18 deg/s, one revolution
        slow_pitch_360.json
        slow_roll_360.json
        fast_yaw_360.json          # 180 deg/s
    multi_axis/
        figure_eight.json          # Smooth 2-axis sinusoidal
        walking_gait.json          # Periodic pitch/roll at 1.8 Hz
    stress/
        high_rate_oscillation.json # 500 dps oscillation
        near_gimbal_lock.json      # Pitch to +/-89 degrees
        impulse_shock.json         # Sudden start/stop
```

---

## 8. Data Export & Visualization

### CSV Format

```csv
time_s,truth_w,truth_x,truth_y,truth_z,fused_w,fused_x,fused_y,fused_z,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,mag_x,mag_y,mag_z,pressure_hpa,error_deg
0.000,1.000,0.000,0.000,0.000,0.999,0.001,...
```

**When exported:** Only when `HELIX_TEST_EXPORT=1` env var is set. Normal CI gate produces no files.

**Output location:** `${CMAKE_BINARY_DIR}/test_output/` (gitignored).

### Python Plot Script (`simulators/scripts/plot_test_run.py`)

~150 lines, matplotlib only (no pandas). Produces 4 standard plots:

1. **Orientation tracking** -- Euler angles: truth (dashed) vs. fused (solid), 3 subplots
2. **Angular error vs. time** -- The hero plot. Shows convergence, drift, spikes
3. **Raw sensor data** -- 3x3 grid (accel/gyro/mag XYZ) for debugging
4. **Error histogram** -- Gaussian = good, heavy-tailed = filter problem

```bash
HELIX_TEST_EXPORT=1 ./build-arm/helix_integration_tests
python3 simulators/scripts/plot_test_run.py build-arm/test_output/*/timeseries.csv
```

### JSON Summary (machine-readable metrics)

```json
{
  "test": "drift_stationary_60s",
  "config": {"kp": 1.0, "ki": 0.0},
  "errors_injected": {"gyro_bias": [0.01, 0, 0]},
  "metrics": {
    "rms_error_deg": 1.1,
    "max_error_deg": 2.3,
    "drift_rate_deg_per_min": 0.4,
    "convergence_time_5deg_s": 0.5
  }
}
```

---

## 9. Metrics & Pass/Fail Criteria

### Core Metrics

| Metric | What it measures | Unit |
|--------|-----------------|------|
| Angular RMS error | Overall tracking accuracy | degrees |
| Max angular error | Worst-case deviation | degrees |
| Drift rate | Gyro integration quality | deg/min |
| Convergence time (T5) | Filter startup | seconds |
| Calibration residual | Post-cal offset | degrees |

### Proposed Thresholds (generous initially, tighten later)

| Scenario | RMS error | Max error | Drift rate |
|----------|-----------|-----------|------------|
| Stationary, clean sensors | < 1 deg | < 3 deg | < 0.5 deg/min |
| Stationary, mild errors | < 3 deg | < 8 deg | < 2 deg/min |
| Slow rotation, clean | < 3 deg | < 10 deg | n/a |
| Slow rotation, mild errors | < 8 deg | < 20 deg | n/a |
| Fast rotation, clean | < 5 deg | < 15 deg | n/a |
| Calibration complete | n/a | < 2 deg | n/a |

### Anti-Flakiness Strategy

1. **Deterministic seeding** (default `rngSeed = 42`) -- identical noise every run
2. **Statistical margin** -- thresholds at 3-sigma above expected value
3. **Multi-seed validation** (slow tier only) -- run with 10 seeds, assert *worst* passes

---

## 10. Implementation Phases

### Phase 1: Foundation [HIGH] -- ~2 sessions

| Task | Description |
|------|-------------|
| SimulationHarness class | Reusable harness with config, stepping, trace recording |
| Angular error metric | `angularErrorDeg()` utility function |
| CSV export | `exportCsv()` on RunResult |
| 4 canonical motion profiles | `flat_60s.json`, `slow_yaw_360.json`, `six_position_tumble.json`, `figure_eight.json` |

### Phase 2: Core Calibration Tests [HIGH] -- ~2 sessions

| Task | Description |
|------|-------------|
| Accel 6-position tumble | Recover bias + scale from 6 poses |
| Gyro static bias estimation | 10s stationary, recover bias |
| Gyro rate scale calibration | Known rotation rate, recover scale |
| Hard iron sphere fit | 3-rotation sphere fit, recover offset |
| Gravity removal stationary test | LinearAccelExtractor at various tilts |

### Phase 3: Filter Validation [HIGH] -- ~2 sessions

| Task | Description |
|------|-------------|
| Convergence from initial error | Snap gimbal, measure recovery |
| Steady-state during rotation | 30 deg/s tracking accuracy |
| Ki bias rejection | Compare Ki=0 vs Ki>0 with injected gyro bias |
| Drift characterization | 60s stationary drift rate |
| Parameterized error sweep | GoogleTest INSTANTIATE_TEST_SUITE_P |

### Phase 4: Magnetometer & Visualization [MEDIUM] -- ~2 sessions

| Task | Description |
|------|-------------|
| Soft iron ellipsoid fit | Extend sphere fit to ellipsoid |
| Tilt-compensated heading | 144-pose heading accuracy matrix |
| Figure-8 tumble + quality scoring | Sphere coverage, fit residual |
| Python plot script | `plot_test_run.py` with 4 standard plots |
| Gain sweep parameterized tests | Kp/Ki Pareto front |

### Phase 5: Advanced [MEDIUM] -- ~2 sessions

| Task | Description |
|------|-------------|
| Magnetic interference rejection | Transient spike test (requires Mahony change) |
| Degraded sensor tests | Mag dropout, high noise, large bias |
| Dynamic motion profiles | Walking gait, arm swing, staircase |
| CI simulation tier | Separate slow job with artifact upload |
| Multi-seed statistical validation | 10-seed worst-case assertion |

---

## 11. Simulator Enhancements Needed

| Enhancement | Unlocks | Priority | Effort |
|-------------|---------|----------|--------|
| `Lsm6dsoSimulator::setLinearAcceleration(Vec3)` | Gravity removal with real linear motion | HIGH | ~20 lines |
| Full 3x3 soft iron matrix in BMM350 | Off-axis ellipsoid testing | LOW | ~30 lines |
| Temperature coefficient on mag (`Vec3 tempCoeff`) | Thermal drift testing | LOW | ~10 lines |
| `VirtualGimbal::setAltitudeRate(float)` | Baro-aided altitude testing | LOW | ~20 lines |
| Gyro random walk / bias instability | Allan variance, realistic drift | MEDIUM | ~40 lines |

---

## Expert Panel Consensus

The four experts agreed on these key points:

1. **Build the SimulationHarness first** -- everything else plugs into it.
2. **Six-position tumble (accel) + static bias (gyro) + sphere fit (mag)** are the three foundational calibration tests.
3. **Mahony Ki bias rejection test** is the single most valuable filter validation -- it directly proves the integral term works.
4. **Deterministic noise seeding** is non-negotiable for CI stability.
5. **CSV export + Python plotting** gives the most debugging value per effort. Inspect graphs to build intuition before hardware arrives.
6. **Keep existing integration tests as-is.** They are simple, readable smoke tests. The harness is a new tier on top.
7. **Start with generous pass/fail thresholds** and tighten as the filter improves.
