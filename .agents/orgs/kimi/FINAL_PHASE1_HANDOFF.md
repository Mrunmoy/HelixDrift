# Final Phase 1 Implementation Handoff

**Document ID:** KIMI-EXP-HANDOFF-001  
**Version:** 1.0 (Final)  
**Date:** 2026-03-29  
**Author:** Kimi (experiment-analysis sidecar)  
**Target:** Implementation agent for sidecar Phase 1  
**Status:** Ready for Immediate Execution

---

## Executive Summary

This is the final, executable handoff for the Python analysis sidecar Phase 1. An implementation agent should be able to execute this without ambiguity.

**Scope:**
- 3 Python files to implement
- 2 test files to write
- 1 chicken-and-egg workflow issue resolved (see Section 4)
- 0 C++ files touched
- 0 simulator changes required

---

## 1. Implementation Checklist

### Pre-Flight (Before Writing Code)

| Step | Action | Verification |
|------|--------|--------------|
| 1 | Create directories | `mkdir -p tools/analysis/tests experiments/runs experiments/analysis` |
| 2 | Verify Python 3.10+ | `python --version` |
| 3 | Install dependencies | `pip install pydantic>=2.0 numpy>=1.24 pytest` |
| 4 | Verify nix develop works | `nix develop --command python --version` |

### File Creation Order

| Order | File | Purpose | Lines (est) |
|-------|------|---------|-------------|
| 1 | `tools/analysis/__init__.py` | Package marker | 0 |
| 2 | `tools/analysis/schema.py` | Pydantic models | ~180 |
| 3 | `tools/analysis/tests/__init__.py` | Test package marker | 0 |
| 4 | `tools/analysis/tests/test_schema.py` | Schema validation tests | ~80 |
| 5 | `tools/analysis/metrics.py` | Statistics computation | ~120 |
| 6 | `tools/analysis/tests/test_metrics.py` | Metrics computation tests | ~90 |
| 7 | `tools/analysis/run_single_analysis.py` | CLI tool | ~150 |

### First Tests to Write (In This Order)

```python
# 1. test_schema.py - verify Manifest creation
from tools.analysis.schema import Manifest, GimbalConfig, Quaternion

def test_minimal_manifest():
    manifest = Manifest(
        experiment_id="test_run",
        gimbal_config=GimbalConfig(
            initial_orientation=Quaternion(w=1, x=0, y=0, z=0)
        )
    )
    assert manifest.schema_version == "1.0"
    assert manifest.experiment_id == "test_run"

# 2. test_metrics.py - verify RMS computation
from tools.analysis.metrics import compute_angular_error_metrics
from tools.analysis.schema import Sample

def test_constant_error_rms():
    samples = [
        Sample(sample_idx=i, timestamp_us=i*20000,
               truth_w=1, truth_x=0, truth_y=0, truth_z=0,
               fused_w=1, fused_x=0, fused_y=0, fused_z=0,
               angular_error_deg=5.0)
        for i in range(10)
    ]
    metrics = compute_angular_error_metrics(samples)
    assert metrics.rms_deg == 5.0  # RMS of constant = constant
```

### First Commands to Run

```bash
# After schema.py exists
cd /home/mrumoy/sandbox/embedded/HelixDrift
python -c "from tools.analysis.schema import Manifest; print('schema OK')"

# After test_schema.py exists
pytest tools/analysis/tests/test_schema.py::test_minimal_manifest -v

# After metrics.py exists
python -c "from tools.analysis.metrics import compute_angular_error_metrics; print('metrics OK')"

# After run_single_analysis.py exists
python -m tools.analysis.run_single_analysis --help
```

### Success Criteria Verification

| Check | Command | Pass Criteria |
|-------|---------|---------------|
| Schema loads | `python -c "from tools.analysis.schema import Manifest, Sample, Summary"` | No ImportError |
| Metrics compute | `pytest tools/analysis/tests/test_metrics.py -v` | All tests pass |
| CLI works | `python -m tools.analysis.run_single_analysis --help` | Shows usage |
| End-to-end | Create test run dir, run analysis | Prints summary table |
| No C++ touched | `git diff --name-only | grep -E '\.(cpp|hpp|c|h)$'` | Empty output |

---

## 2. File-by-File Phase 1 Specifications

### File 1: `tools/analysis/schema.py`

**Purpose:** Pydantic v2 models for type-safe JSON/CSV handling

**Key Classes:**
- `Quaternion` (w, x, y, z floats)
- `Vector3` (x, y, z floats)
- `SimulatorConfig` (mahony_kp, mahony_ki, seed, output_period_us)
- `Manifest` (run metadata - see example below)
- `Sample` (one CSV row - see example below)
- `AngularErrorMetrics` (rms, max, final, mean, std, p95, p99)
- `DriftAnalysis` (drift_rate, convergence_time)
- `Summary` (aggregated results - **computed by Python**, not loaded from file)
- `RunResult` (container with manifest, samples, summary)

**CRITICAL FIX from original pack:**

The original `RunResult.from_directory()` tried to load `summary.json` which creates a chicken-and-egg problem. Use this corrected version:

```python
class RunResult(BaseModel):
    """Complete result of a single experiment run."""
    manifest: Manifest
    samples: List[Sample]
    summary: Optional[Summary] = None  # Computed post-hoc, not loaded
    
    @classmethod
    def from_raw_directory(cls, run_dir: Path) -> "RunResult":
        """
        Load raw run data from directory (manifest + samples only).
        Summary is computed later by analysis tools.
        """
        import json
        import csv
        
        # Load manifest
        with open(run_dir / "manifest.json") as f:
            manifest = Manifest(**json.load(f))
        
        # Load samples
        samples = []
        with open(run_dir / "samples.csv") as f:
            reader = csv.DictReader(f)
            for row in reader:
                # Robust type coercion: try float, fall back to int
                def parse_value(k: str, v: str):
                    if k == 'sample_idx':
                        return int(v)
                    try:
                        return float(v)
                    except ValueError:
                        return v
                
                samples.append(Sample(**{k: parse_value(k, v) 
                                        for k, v in row.items()}))
        
        return cls(manifest=manifest, samples=samples, summary=None)
```

### File 2: `tools/analysis/metrics.py`

**Purpose:** Compute statistics from raw samples

**Key Functions:**

```python
def compute_angular_error_metrics(samples: List[Sample]) -> AngularErrorMetrics:
    """Compute RMS, max, mean, std, p95, p99 from sample errors."""
    errors = np.array([s.angular_error_deg for s in samples])
    if len(errors) == 0:
        return AngularErrorMetrics(rms_deg=0.0, max_deg=0.0, ...)
    
    rms = np.sqrt(np.mean(errors ** 2))
    return AngularErrorMetrics(
        rms_deg=float(rms),
        max_deg=float(np.max(errors)),
        final_deg=float(errors[-1]),
        mean_deg=float(np.mean(errors)),
        std_deg=float(np.std(errors)),
        p95_deg=float(np.percentile(errors, 95)),
        p99_deg=float(np.percentile(errors, 99))
    )

def compute_drift_rate_simple(samples: List[Sample], 
                              method: str = "endpoints") -> float:
    """Compute drift rate in degrees per minute."""
    if len(samples) < 2:
        return 0.0
    
    errors = np.array([s.angular_error_deg for s in samples])
    timestamps = np.array([s.timestamp_us for s in samples])
    duration_min = (timestamps[-1] - timestamps[0]) / (1_000_000 * 60)
    
    if duration_min <= 0:
        return 0.0
    
    if method == "endpoints":
        drift = (errors[-1] - errors[0]) / duration_min
    elif method == "regression":
        timestamps_min = (timestamps - timestamps[0]) / (1_000_000 * 60)
        slope, _ = np.polyfit(timestamps_min, errors, 1)
        drift = slope
    else:
        raise ValueError(f"Unknown method: {method}")
    
    return float(drift)

def detect_convergence(samples: List[Sample],
                      threshold_deg: float = 3.0,
                      window_size: int = 10) -> Optional[float]:
    """Time to first enter and stay below threshold (seconds)."""
    if len(samples) < window_size:
        return None
    
    errors = np.array([s.angular_error_deg for s in samples])
    timestamps = np.array([s.timestamp_us for s in samples])
    
    for i in range(len(errors) - window_size + 1):
        if np.all(errors[i:i+window_size] < threshold_deg):
            return float(timestamps[i + window_size - 1] - timestamps[0]) / 1_000_000
    
    return None
```

### File 3: `tools/analysis/run_single_analysis.py`

**Purpose:** CLI tool to analyze a single run and produce summary.json

**Workflow:**
1. Load manifest.json + samples.csv (raw data from C++ test)
2. Compute metrics using metrics.py
3. Generate Summary object
4. Print human-readable summary to stdout
5. Write summary.json to disk (optional)

**CLI Interface:**

```bash
python -m tools.analysis.run_single_analysis \
    experiments/runs/20260329/A1_static_pose/run_001 \
    --output-summary experiments/runs/20260329/A1_static_pose/run_001/summary.json \
    --criteria '{"rms_threshold_deg": 5.0, "max_threshold_deg": 10.0}'
```

**Exit codes:**
- 0: Analysis succeeded, all acceptance criteria passed
- 1: Analysis succeeded, but criteria failed
- 2: Error (file not found, parse error, etc.)

---

## 3. Test Files

### `tools/analysis/tests/test_schema.py`

```python
"""Tests for schema validation."""

import pytest
from pathlib import Path
import json

from tools.analysis.schema import (
    Manifest, Sample, Summary, RunResult,
    SimulatorConfig, GimbalConfig, Quaternion
)


class TestManifest:
    def test_minimal_manifest(self):
        manifest = Manifest(
            experiment_id="test_run",
            gimbal_config=GimbalConfig(
                initial_orientation=Quaternion(w=1, x=0, y=0, z=0)
            )
        )
        assert manifest.schema_version == "1.0"
        assert manifest.experiment_id == "test_run"
    
    def test_full_manifest(self):
        manifest = Manifest(
            experiment_id="A1_test",
            experiment_family="Wave_A",
            description="Test run",
            simulator_config=SimulatorConfig(mahony_kp=2.0, seed=123),
            gimbal_config=GimbalConfig(
                initial_orientation=Quaternion(w=0.707, x=0, y=0, z=0.707)
            )
        )
        assert manifest.simulator_config.mahony_kp == 2.0


class TestSample:
    def test_sample_creation(self):
        sample = Sample(
            sample_idx=0,
            timestamp_us=1000000,
            truth_w=1.0, truth_x=0, truth_y=0, truth_z=0,
            fused_w=0.99, fused_x=0.1, fused_y=0, fused_z=0,
            angular_error_deg=1.5
        )
        assert sample.sample_idx == 0
        assert sample.angular_error_deg == 1.5


class TestRunResult:
    def test_load_from_raw_directory(self, tmp_path: Path):
        """Load raw run data (manifest + samples, no summary yet)."""
        # Create manifest
        manifest = {
            "experiment_id": "test",
            "gimbal_config": {
                "initial_orientation": {"w": 1, "x": 0, "y": 0, "z": 0}
            }
        }
        with open(tmp_path / "manifest.json", "w") as f:
            json.dump(manifest, f)
        
        # Create samples.csv
        with open(tmp_path / "samples.csv", "w") as f:
            f.write("sample_idx,timestamp_us,truth_w,truth_x,truth_y,truth_z,fused_w,fused_x,fused_y,fused_z,angular_error_deg\n")
            f.write("0,1000000,1,0,0,0,0.99,0.1,0,0,1.5\n")
        
        # Load and verify
        result = RunResult.from_raw_directory(tmp_path)
        assert result.manifest.experiment_id == "test"
        assert len(result.samples) == 1
        assert result.summary is None  # Not computed yet
```

### `tools/analysis/tests/test_metrics.py`

```python
"""Tests for metrics computation."""

import pytest
import numpy as np

from tools.analysis.metrics import (
    compute_angular_error_metrics,
    compute_drift_rate_simple,
    detect_convergence
)
from tools.analysis.schema import Sample


class TestAngularErrorMetrics:
    def test_empty_samples(self):
        metrics = compute_angular_error_metrics([])
        assert metrics.rms_deg == 0.0
        assert metrics.max_deg == 0.0
    
    def test_constant_error(self):
        samples = [
            Sample(sample_idx=i, timestamp_us=i*20000,
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=5.0)
            for i in range(10)
        ]
        metrics = compute_angular_error_metrics(samples)
        assert metrics.rms_deg == 5.0
        assert metrics.mean_deg == 5.0
        assert metrics.max_deg == 5.0
    
    def test_varying_error(self):
        errors = [3.0, 4.0, 5.0]  # RMS = sqrt((9+16+25)/3) ≈ 4.08
        samples = [
            Sample(sample_idx=i, timestamp_us=i*20000,
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=e)
            for i, e in enumerate(errors)
        ]
        metrics = compute_angular_error_metrics(samples)
        expected_rms = np.sqrt(np.mean([e**2 for e in errors]))
        assert pytest.approx(metrics.rms_deg, 0.01) == expected_rms


class TestDriftRate:
    def test_no_drift(self):
        samples = [
            Sample(sample_idx=i, timestamp_us=i*1000000,  # 1 second intervals
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=5.0)
            for i in range(60)
        ]
        drift = compute_drift_rate_simple(samples)
        assert pytest.approx(drift, 0.01) == 0.0
    
    def test_linear_drift(self):
        # Error increases 1 degree per minute
        samples = [
            Sample(sample_idx=i, timestamp_us=i*1000000,
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=i/60.0)
            for i in range(60)
        ]
        drift = compute_drift_rate_simple(samples)
        assert pytest.approx(drift, 0.1) == 1.0


class TestConvergence:
    def test_converged(self):
        errors = [10.0, 8.0, 6.0, 4.0, 2.0, 1.0, 1.0, 1.0, 1.0, 1.0]
        samples = [
            Sample(sample_idx=i, timestamp_us=i*1000000,
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=e)
            for i, e in enumerate(errors)
        ]
        conv_time = detect_convergence(samples, threshold_deg=3.0, window_size=3)
        assert conv_time is not None
        assert conv_time > 0
    
    def test_not_converged(self):
        samples = [
            Sample(sample_idx=i, timestamp_us=i*1000000,
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=10.0)
            for i in range(10)
        ]
        conv_time = detect_convergence(samples, threshold_deg=3.0)
        assert conv_time is None
```

---

## 4. Corrected Artifact-Loading Workflow (Chicken-and-Egg Fixed)

### The Problem

Original spec had `RunResult.from_directory()` loading:
1. manifest.json
2. samples.csv  
3. **summary.json** ← Problem: this doesn't exist yet when analysis starts

### The Corrected Workflow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Experiment Run Workflow                              │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  C++ TEST (Codex)                                                        │
│  ├── Run simulation with VirtualMocapNodeHarness                       │
│  ├── Capture samples (truth/fused/error)                                │
│  ├── Write manifest.json (metadata)                                     │
│  └── Write samples.csv (time-series data)                               │
│                                                                          │
│                         ↓                                                │
│                                                                          │
│  PYTHON ANALYSIS (Sidecar)                                               │
│  ├── Load manifest.json + samples.csv via RunResult.from_raw_directory │
│  ├── Compute metrics (RMS, drift, convergence)                          │
│  ├── Build Summary object                                               │
│  ├── Print human-readable report                                        │
│  └── Write summary.json (aggregated metrics) ← Optional output          │
│                                                                          │
│                         ↓                                                │
│                                                                          │
│  LLM SUMMARIZATION (Phase 3)                                             │
│  ├── Load manifest.json + summary.json                                  │
│  └── Generate narrative.txt                                             │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### File Roles

| File | Produced By | Consumed By | Contains |
|------|-------------|-------------|----------|
| `manifest.json` | C++ test | Python analysis | Run metadata (config, parameters) |
| `samples.csv` | C++ test | Python analysis | Time-series data (per-sample truth/fused/error) |
| `summary.json` | Python analysis | LLM summarizer | Aggregated metrics (RMS, drift, convergence) |
| `narrative.txt` | LLM (Phase 3) | Humans | Natural language summary |

### Key Change Summary

| Original (Broken) | Corrected (Working) |
|-------------------|---------------------|
| `RunResult.from_directory()` loads summary.json | `RunResult.from_raw_directory()` loads only manifest + samples |
| Creates chicken-and-egg | Summary is computed post-hoc |
| Summary assumed pre-existing | Summary is output of analysis |

---

## 5. Priority Changes After SensorFusion Fix

### Did Priorities Change?

**Partially. The SensorFusion AHRS convention fix (merged) changes the A1 priority.**

### Pre-Fix Status (from DEV_JOURNAL)

| Test | Status Before Fix | RMS Error |
|------|-------------------|-----------|
| A1 identity | ✅ Perfect | ~0° |
| A1 ±90° yaw | ❌ Catastrophic failure | ~118° RMS |
| A5 bias rejection | ✅ Working | Validated |

### Post-Fix Status (Expected)

| Test | Status After Fix | Impact |
|------|------------------|--------|
| A1 identity | ✅ Still perfect | Unchanged |
| A1 ±90° yaw | ⚠️ Needs re-evaluation | Previously blocked by convention bug |
| A5 bias rejection | ✅ Still working | Unchanged |

### Updated Priority Matrix

| Priority | Batch | Runs | Effort | Blocked By | Notes |
|----------|-------|------|--------|------------|-------|
| 🔴 P0 | Wave A Baseline | 45 | 1h | A1-A6 tests | A1 may now be unblocked |
| 🔴 P0 | Mahony Sweep | 36 | 2h | A4 test | Unchanged |
| 🟡 P1 | Bias Sensitivity | 60 | 3h | A5 test | Unchanged |
| 🟡 P1 | Seed Sensitivity | 20 | 4h | None | Unchanged |
| 🟢 P2 | Noise Sensitivity | TBD | 4h | B4 gaps | Unchanged |
| 🟢 P2 | Long Duration | TBD | 8h | A3 test | Unchanged |

### Specific Change: A1 Re-evaluation Recommended

**Claude's staged A1 entry path (from WAVE_A_ACCEPTANCE_GUIDE) should now be re-probed:**

```
Before fix: ±90° yaw was ~118° RMS (catastrophic, architecture limitation)
After fix:  ±90° yaw may be within intermediate threshold (~5-15° RMS)
```

**Recommendation:** 
- Run A1 staged probe again (identity, +90° yaw, -90° yaw)
- If now within intermediate threshold, A1 can proceed to P0
- If still failing, remains blocked for M2 closure discussion

### No Other Priority Changes

The SensorFusion fix was specifically about AHRS convention mismatch. It does not affect:
- A2 dynamic tracking (separate concern)
- A3 long drift (time-dependent, not initialization)
- A4 Mahony sweep (parameter space exploration)
- A5 bias rejection (already validated)
- A6 joint angle (two-node, separate concern)

---

## 6. One-Page Note for Codex/Implementation Agent

### Before You Start

**Mission:** Build Python analysis sidecar Phase 1. Consume C++ test output. Produce metrics and plots. Do not touch simulator.

**Files You Create:**
- `tools/analysis/schema.py` — Pydantic models
- `tools/analysis/metrics.py` — Statistics functions  
- `tools/analysis/run_single_analysis.py` — CLI tool
- `tools/analysis/tests/test_schema.py` — Validation tests
- `tools/analysis/tests/test_metrics.py` — Computation tests

**Files You Do NOT Touch:**
- Nothing in `simulators/`
- Nothing in `firmware/`
- Nothing in `tests/*.cpp`
- No C++ files anywhere

**Key Workflow Fix:**
- `summary.json` is OUTPUT of Python analysis, not INPUT
- Use `RunResult.from_raw_directory()` to load manifest + samples
- Compute summary, then optionally write summary.json

**First Milestone:**
```bash
python -m tools.analysis.run_single_analysis \
    /path/to/run_dir \
    --output-summary /path/to/summary.json
# Should print readable summary table
```

**Integration with Codex Work:**
- Codex produces: `manifest.json` + `samples.csv` (via C++ test export)
- You consume: `manifest.json` + `samples.csv`
- You produce: `summary.json` + plots + reports

**Questions?**
- Check `PHASE1_IMPLEMENTATION_PACK.md` for detailed specs
- Check `EXPERIMENT_RESULT_SCHEMA.md` for JSON/CSV format
- Check `BATCH_PIPELINE_ARCHITECTURE.md` for future Phase 2 context

**Phase 1 Complete When:**
- [ ] All 5 Python files implemented
- [ ] All tests pass (`pytest tools/analysis/tests/`)
- [ ] CLI works (`--help` shows usage)
- [ ] Can analyze example run and print summary
- [ ] No C++ files modified (verify with `git diff`)

---

## 7. Summary

| Deliverable | Status |
|-------------|--------|
| Executable checklist | ✅ Section 1 |
| File-by-file specs | ✅ Section 2 |
| Test file specs | ✅ Section 3 |
| Workflow fix documented | ✅ Section 4 |
| Priority changes assessed | ✅ Section 5 (A1 may be unblocked) |
| One-page agent note | ✅ Section 6 |

**Next Action:** Hand off to implementation agent. No further planning needed.

**Blocking Issues:** None. Ready for immediate execution.
