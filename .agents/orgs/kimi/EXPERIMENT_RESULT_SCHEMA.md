# Experiment Result Schema

**Document ID:** KIMI-EXP-SCHEMA-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (experiment-analysis sidecar)  
**Target:** Deterministic simulation output format for Python/Ollama analysis  
**Status:** Design Complete

---

## Overview

This schema defines the deterministic output format for HelixDrift simulation experiments. It is designed to be:
- **Machine-parseable** (JSON Lines + CSV)
- **Human-inspectable** (flat structure, readable field names)
- **LLM-friendly** (natural language descriptions embedded)
- **Git-diffable** (deterministic ordering, no timestamps in filenames)

---

## Directory Structure

```
experiments/
├── runs/                           # Individual experiment runs
│   ├── 20260329/                   # Date prefix for organization
│   │   ├── A1_static_pose/         # Wave A1 experiment
│   │   │   ├── run_001/            # Individual run
│   │   │   │   ├── manifest.json   # Run metadata
│   │   │   │   ├── samples.csv     # Per-sample data
│   │   │   │   └── summary.json    # Aggregated metrics
│   │   │   ├── run_002/
│   │   │   └── run_003/
│   │   ├── A4_kp_ki_sweep/
│   │   │   ├── kp0.5_ki0.0/
│   │   │   ├── kp1.0_ki0.0/
│   │   │   └── ...
│   │   └── A5_bias_rejection/
│   └── 20260330/
├── batches/                        # Batch experiment definitions
│   ├── wave_A_batch.json
│   ├── mahony_sweep_batch.json
│   └── sensor_noise_sensitivity.json
└── analysis/                       # Analysis outputs (gitignored)
    ├── plots/
    ├── llm_summaries/
    └── regression_reports/
```

---

## Schema Definitions

### 1. Manifest (`manifest.json`)

Run-level metadata. Required for every experiment run.

```json
{
    "schema_version": "1.0",
    "experiment_id": "A1_static_pose_yaw_90",
    "experiment_family": "Wave_A",
    "description": "Static pose test at 90 degree yaw with default Mahony tuning",
    
    "timestamp_iso": "2026-03-29T10:30:00Z",
    "hostname": "workstation-01",
    "git_commit": "f9295c6",
    
    "simulator_config": {
        "mahony_kp": 1.0,
        "mahony_ki": 0.02,
        "seed": 42,
        "output_period_us": 20000
    },
    
    "gimbal_config": {
        "initial_orientation": {"w": 0.707, "x": 0, "y": 0, "z": 0.707},
        "rotation_rate_dps": {"x": 0, "y": 0, "z": 0},
        "motion_profile": "static"
    },
    
    "sensor_errors": {
        "gyro_bias_rad_s": {"x": 0, "y": 0, "z": 0},
        "gyro_noise_std": 0.001,
        "accel_bias_g": {"x": 0, "y": 0, "z": 0},
        "mag_hard_iron_ut": {"x": 0, "y": 0, "z": 0}
    },
    
    "execution": {
        "warmup_samples": 50,
        "measured_samples": 200,
        "total_ticks": 250,
        "duration_seconds": 5.0
    }
}
```

**Required Fields:**
- `schema_version`: Always "1.0"
- `experiment_id`: Unique identifier for this specific run
- `experiment_family`: Grouping (Wave_A, Wave_B, calibration, etc.)
- `simulator_config`: All tunable parameters
- `execution`: Sample counts and duration

**Optional but Recommended:**
- `sensor_errors`: What errors were injected
- `git_commit`: For reproducibility
- `description`: Human-readable purpose

---

### 2. Samples (`samples.csv`)

Per-sample time-series data. One row per fusion output.

```csv
sample_idx,timestamp_us,truth_w,truth_x,truth_y,truth_z,fused_w,fused_x,fused_y,fused_z,angular_error_deg,error_axis_x,error_axis_y,error_axis_z
0,1000000,0.7071,0.0000,0.0000,0.7071,0.7050,0.0100,0.0050,0.7090,1.234,0.1,0.2,0.9
1,1020000,0.7071,0.0000,0.0000,0.7071,0.7060,0.0080,0.0040,0.7080,0.987,0.1,0.2,0.9
...
```

**Field Definitions:**

| Field | Type | Description |
|-------|------|-------------|
| `sample_idx` | int | Sequential sample number (0-indexed) |
| `timestamp_us` | int | Microseconds since run start |
| `truth_w/x/y/z` | float | Ground truth quaternion (normalized) |
| `fused_w/x/y/z` | float | Fused output quaternion (normalized) |
| `angular_error_deg` | float | Angular error in degrees |
| `error_axis_x/y/z` | float | Axis of error rotation (normalized) |

**Optional Additional Columns (for deep debugging):**
- `accel_x/y/z`: Raw accelerometer (g)
- `gyro_x/y/z`: Raw gyroscope (rad/s)
- `mag_x/y/z`: Raw magnetometer (µT)

---

### 3. Summary (`summary.json`)

Aggregated metrics. Computed by C++ or Python post-processing.

```json
{
    "sample_count": 200,
    "duration_seconds": 4.0,
    
    "angular_error": {
        "rms_deg": 2.34,
        "max_deg": 5.67,
        "final_deg": 1.89,
        "mean_deg": 2.12,
        "std_deg": 0.89,
        "p95_deg": 3.45,
        "p99_deg": 4.56
    },
    
    "drift_analysis": {
        "drift_rate_deg_per_min": 0.45,
        "drift_rate_computed_from": "first_10_to_last_10_samples",
        "convergence_time_seconds": 1.5,
        "convergence_threshold_deg": 3.0
    },
    
    "acceptance_criteria": {
        "passed": true,
        "checks": {
            "rms_lt_3_deg": {"threshold": 3.0, "actual": 2.34, "passed": true},
            "max_lt_10_deg": {"threshold": 10.0, "actual": 5.67, "passed": true},
            "drift_lt_2_deg_per_min": {"threshold": 2.0, "actual": 0.45, "passed": true}
        }
    },
    
    "statistics": {
        "warmup_discarded": 50,
        "outliers_removed": 0,
        "computation_timestamp": "2026-03-29T10:30:05Z"
    }
}
```

---

## Batch Definition Schema (`batch.json`)

Defines a parameter sweep or comparison experiment.

```json
{
    "schema_version": "1.0",
    "batch_id": "A4_kp_ki_sweep",
    "description": "Mahony Kp/Ki convergence sweep for M2 validation",
    
    "matrix": {
        "mahony_kp": [0.5, 1.0, 2.0, 5.0],
        "mahony_ki": [0.0, 0.02, 0.05]
    },
    
    "fixed_parameters": {
        "seed": 42,
        "warmup_samples": 50,
        "measured_samples": 500,
        "motion_profile": "snap_90_yaw",
        "gyro_bias_rad_s": {"x": 0, "y": 0, "z": 0}
    },
    
    "execution": {
        "parallel_runs": 4,
        "deterministic_order": true
    },
    
    "analysis": {
        "compare_across": ["mahony_kp", "mahony_ki"],
        "metrics_of_interest": ["rms_error", "convergence_time", "steady_state_error"],
        "generate_plots": ["convergence_curves", "param_heatmap"]
    }
}
```

---

## Determinism Requirements

### 1. Seed Propagation
- Every random element must use `setSeed()` from manifest
- Same seed → identical samples.csv byte-for-byte

### 2. Ordering
- Samples written in temporal order
- No sorting that could break determinism
- Batch runs executed in lexicographic order of parameter combinations

### 3. Floating Point
- Use `std::fixed` + `std::setprecision(6)` for CSV output
- Accept small diffs from math library variations across platforms

---

## LLM-Friendly Additions

### Natural Language Summary (`narrative.txt`)

Auto-generated text description for LLM consumption:

```
Experiment: A1_static_pose_yaw_90
Family: Wave_A (M2 validation)

This run tested static orientation accuracy at 90 degrees yaw with default
Mahony tuning (Kp=1.0, Ki=0.02). The gimbal was stationary for 5 seconds
after a 50-tick warmup period.

Key Results:
- RMS angular error: 2.34 degrees (threshold: < 3.0, PASSED)
- Maximum error: 5.67 degrees (threshold: < 10.0, PASSED)
- Final error: 1.89 degrees
- Drift rate: 0.45 deg/min (threshold: < 2.0, PASSED)

All acceptance criteria passed. The fusion converged within 1.5 seconds
and maintained steady-state accuracy within specification.

Configuration Notes:
- Deterministic seed: 42
- No sensor errors injected (clean run)
- 50 Hz output rate (20ms period)
- 250 total samples, 200 measured after warmup
```

---

## Anti-Scope-Creep Notes

### What This Schema Does NOT Include:
- ❌ Raw sensor register dumps (too verbose)
- ❌ Video/animation data (out of scope)
- ❌ Binary protobuf (human readability prioritized)
- ❌ Real-time streaming (batch processing only)
- ❌ Cloud storage URIs (local filesystem first)

### What Codex Does NOT Need to Change:
- ❌ Existing test assertions (this is additive export)
- ❌ C++ test structure (export is side effect)
- ❌ Simulator timing (determinism already required)

---

**Status:** ✅ Schema ready for implementation
