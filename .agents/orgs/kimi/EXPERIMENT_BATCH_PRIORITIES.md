# Experiment Batch Priorities

**Document ID:** KIMI-EXP-PRIORITY-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (experiment-analysis sidecar)  
**Target:** First batches to run when pipeline is ready  
**Status:** Ready for Execution

---

## Priority Matrix

| Priority | Batch | Value | Effort | Blocked By | When to Run |
|----------|-------|-------|--------|------------|-------------|
| 🔴 P0 | Wave A Baseline | High | 1h | A1-A6 tests | As tests complete |
| 🔴 P0 | Mahony Sweep | High | 2h | A4 test | After A4 passes |
| 🟡 P1 | Bias Sensitivity | Med | 3h | A5 test | After A5 passes |
| 🟡 P1 | Seed Sensitivity | Med | 4h | setSeed() | Anytime |
| 🟢 P2 | Noise Sensitivity | Low | 4h | B4 gaps | Wave B |
| 🟢 P2 | Long Duration | Low | 8h | A3 test | Low priority |

---

## P0: Critical Path Batches

### Batch 1: Wave A Baseline Collection

**Purpose:** Establish reproducible baseline for all M2 experiments  
**Blocked By:** A1-A6 tests passing (Codex work)  
**Effort:** ~1 hour compute time  
**Value:** Foundation for all future comparison

**Definition:**
```json
{
    "batch_id": "wave_a_baseline",
    "description": "Re-run all Wave A experiments with export enabled",
    "runs": [
        {"experiment": "A1_static_pose", "poses": ["identity", "yaw_90", "yaw_neg90", "pitch_45", "pitch_neg45", "roll_30", "roll_neg30"]},
        {"experiment": "A2_dynamic_tracking", "axes": ["yaw", "pitch", "roll"], "rate_dps": 30},
        {"experiment": "A3_long_drift", "duration_s": 60},
        {"experiment": "A4_mahony_sweep", "kp": [1.0], "ki": [0.02], "subset": "baseline_only"},
        {"experiment": "A5_bias_rejection", "ki": [0.0, 0.05, 0.1]},
        {"experiment": "A6_joint_angle", "flexion": [0, 30, 60, 90, 120]}
    ],
    "repetitions": 3,
    "fixed_parameters": {"seed": 42, "warmup": 50}
}
```

**Outputs:**
- 3 runs × 15 experiments = 45 total runs
- ~200 MB data (CSV + JSON)
- Baseline metrics for comparison

**LLM Analysis:**
- Generate narrative summaries for each run
- Compare across 3 repetitions (check reproducibility)
- Flag any runs with >10% variance from median

---

### Batch 2: Mahony Kp/Ki Full Sweep

**Purpose:** Map parameter space for tuning recommendations  
**Blocked By:** A4 test passing  
**Effort:** ~2 hours compute time (36 runs)  
**Value:** Informs optimal tuning for M3

**Definition:**
```json
{
    "batch_id": "mahony_full_sweep",
    "description": "Complete Kp/Ki parameter space exploration",
    "matrix": {
        "mahony_kp": [0.1, 0.5, 1.0, 2.0, 5.0, 10.0],
        "mahony_ki": [0.0, 0.01, 0.02, 0.05, 0.1, 0.2]
    },
    "fixed_parameters": {
        "seed": 42,
        "warmup": 50,
        "measured": 500,
        "motion": "snap_90_yaw",
        "convergence_threshold_deg": 3.0
    },
    "metrics_of_interest": [
        "convergence_time_seconds",
        "steady_state_rms",
        "overshoot_deg",
        "stability"
    ]
}
```

**Outputs:**
- 6 × 6 = 36 parameter combinations
- Heatmaps: RMS vs Kp/Ki
- Convergence time vs Kp/Ki
- LLM recommendation for "best" trade-off

**LLM Analysis:**
- Triage: identify unstable regions (high Kp + high Ki)
- Compare: find Pareto frontier (fast convergence + low steady-state error)
- Suggest: optimal Kp/Ki for different use cases (VR vs animation)

---

## P1: Important Batches

### Batch 3: Gyro Bias Sensitivity

**Purpose:** Quantify bias rejection across axes and magnitudes  
**Blocked By:** A5 test passing  
**Effort:** ~3 hours (60 runs)  
**Value:** Validates adversarial review findings

**Definition:**
```json
{
    "batch_id": "gyro_bias_sensitivity",
    "description": "Test bias rejection on X, Y, Z axes at multiple magnitudes",
    "matrix": {
        "gyro_bias_axis": ["x", "y", "z"],
        "gyro_bias_rad_s": [0.001, 0.005, 0.01, 0.02, 0.05],
        "mahony_ki": [0.0, 0.02, 0.05, 0.1]
    },
    "fixed_parameters": {
        "seed": 42,
        "mahony_kp": 1.0,
        "duration_s": 30,
        "warmup": 50
    }
}
```

**Key Question:** Does Z-axis bias (tested in A5) show better rejection than X/Y? (Adversarial review hypothesis)

**LLM Analysis:**
- Compare X vs Y vs Z effectiveness
- Identify Ki threshold for each bias magnitude
- Recommend minimum Ki for wearable use case

---

### Batch 4: Seed Sensitivity Analysis

**Purpose:** Verify deterministic behavior across random seeds  
**Blocked By:** None (can run anytime)  
**Effort:** ~4 hours (100 runs)  
**Value:** Validates simulator determinism

**Definition:**
```json
{
    "batch_id": "seed_sensitivity",
    "description": "Run same experiment with different RNG seeds",
    "matrix": {
        "seed": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 
                 42, 100, 123, 999, 12345,
                 55555, 99999, 100000, 314159, 271828]
    },
    "fixed_parameters": {
        "experiment": "A1_static_pose_identity",
        "mahony_kp": 1.0,
        "mahony_ki": 0.02,
        "sensor_noise_enabled": true
    },
    "variance_threshold": 0.1
}
```

**Outputs:**
- Distribution of RMS errors across seeds
- Coefficient of variation (should be <5%)
- Any outlier seeds (potential simulator issues)

**LLM Analysis:**
- Summarize variance statistics
- Flag any seeds producing anomalous results
- Confirm: "Simulator is deterministic and stable across seeds"

---

## P2: Deferred Batches

### Batch 5: Sensor Noise Sensitivity

**Purpose:** Map performance vs noise levels  
**Blocked By:** B4 sensor validation matrix gaps  
**Effort:** ~4 hours  
**When:** Wave B

**Definition:**
```json
{
    "batch_id": "noise_sensitivity",
    "description": "Test degradation with increasing sensor noise",
    "matrix": {
        "gyro_noise_std": [0.0, 0.001, 0.002, 0.005, 0.01],
        "accel_noise_std": [0.0, 0.001, 0.002, 0.005, 0.01],
        "mag_noise_std": [0.0, 0.5, 1.0, 2.0, 5.0]
    },
    "fixed_parameters": {
        "seed": 42,
        "mahony_kp": 1.0,
        "mahony_ki": 0.02
    }
}
```

---

### Batch 6: Long Duration Drift Collection

**Purpose:** Characterize drift over 10+ minutes  
**Blocked By:** A3 test  
**Effort:** ~8 hours (mostly waiting)  
**When:** Low priority (after M2 closes)

**Definition:**
```json
{
    "batch_id": "long_duration_drift",
    "description": "Extended drift characterization",
    "matrix": {
        "duration_minutes": [1, 5, 10, 30],
        "mahony_ki": [0.0, 0.02, 0.05]
    },
    "fixed_parameters": {
        "seed": 42,
        "mahony_kp": 1.0,
        "motion": "static"
    }
}
```

---

## Execution Schedule

### Week 1 (Current)
- Codex: Complete Wave A tests
- Kimi: Implement batch pipeline (Phase 1)

### Week 2
- Codex: A4, A5 passing
- Kimi: Run Batch 1 (Wave A Baseline)
- Kimi: Run Batch 2 (Mahony Sweep)

### Week 3
- Codex: Wave B starts
- Kimi: Run Batch 3 (Bias Sensitivity)
- Kimi: Run Batch 4 (Seed Sensitivity)
- Kimi: LLM analysis of all P0/P1 batches

### Week 4+
- Codex: Continue Wave B
- Kimi: Run P2 batches as time permits
- Kimi: Generate comprehensive report

---

## Resource Requirements

### Compute
| Batch | Runs | Duration | CPU Cores | Disk |
|-------|------|----------|-----------|------|
| Batch 1 | 45 | 1h | 4 | 200 MB |
| Batch 2 | 36 | 2h | 4 | 150 MB |
| Batch 3 | 60 | 3h | 4 | 300 MB |
| Batch 4 | 20 | 4h | 8 | 100 MB |
| **Total P0/P1** | **161** | **10h** | **4-8** | **750 MB** |

### LLM (Optional)
| Model | RAM | Time per Summary | Total Time (161 runs) |
|-------|-----|------------------|----------------------|
| llama3.2:3b | 4GB | 2s | 5 min |
| qwen2.5:7b | 8GB | 5s | 15 min |

---

## Success Criteria

### Batch 1 (Wave A Baseline)
- ✅ All 45 runs complete without error
- ✅ Reproducibility: 3 repetitions within 10% variance
- ✅ LLM summaries for all runs

### Batch 2 (Mahony Sweep)
- ✅ Heatmap generated (RMS vs Kp/Ki)
- ✅ Optimal region identified (Kp: 0.5-2.0, Ki: 0.02-0.05)
- ✅ LLM recommendation matches empirical results

### Batch 3 (Bias Sensitivity)
- ✅ Confirms A5 results (Z-bias rejection works)
- ✅ X/Y bias characterization complete
- ✅ Minimum Ki recommendation for wearables

---

## Anti-Scope-Creep

### These Batches Do NOT:
- ❌ Test RF/sync (wait for M4)
- ❌ Test magnetic disturbance (wait for M5)
- ❌ Test temperature effects (not in sim)
- ❌ Test multi-node (wait for M6)

### These Batches DO:
- ✅ Exercise current simulator capabilities
- ✅ Generate data for M2 validation
- ✅ Inform M3 tuning decisions
- ✅ Validate adversarial review findings

---

**Status:** ✅ Priority list ready — start with Batch 1 when A1-A6 complete
