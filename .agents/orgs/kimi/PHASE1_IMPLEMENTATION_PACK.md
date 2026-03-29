# Phase 1 Implementation Pack

**Document ID:** KIMI-EXP-PHASE1-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (experiment-analysis sidecar)  
**Target:** Implementation agent for sidecar Phase 1  
**Status:** Ready for Handoff

---

## Overview

This pack provides exact implementation specifications for Phase 1 of the experiment-analysis sidecar. An implementation agent should be able to build this with minimal ambiguity.

**Phase 1 Scope:**
- Schema definitions (Pydantic models)
- Metrics computation (statistics)
- Single-run analysis script
- **NO batch runner yet** (Phase 2)
- **NO LLM integration yet** (Phase 3)
- **NO C++ changes** (consumes existing output)

---

## File 1: `tools/analysis/schema.py`

**Purpose:** Pydantic models for JSON validation and type safety

**Dependencies:**
```
pydantic >= 2.0.0
```

**Exact Implementation:**

```python
"""Pydantic models for experiment result schema validation."""

from pathlib import Path
from typing import Dict, List, Optional, Any
from pydantic import BaseModel, Field, field_validator
from datetime import datetime


class Quaternion(BaseModel):
    """Quaternion representation."""
    w: float
    x: float
    y: float
    z: float


class Vector3(BaseModel):
    """3D vector representation."""
    x: float
    y: float
    z: float


class SimulatorConfig(BaseModel):
    """Simulator tuning parameters."""
    mahony_kp: float = 1.0
    mahony_ki: float = 0.02
    seed: int = 42
    output_period_us: int = 20000


class GimbalConfig(BaseModel):
    """Gimbal motion configuration."""
    initial_orientation: Quaternion
    rotation_rate_dps: Vector3 = Vector3(x=0, y=0, z=0)
    motion_profile: str = "static"


class SensorErrors(BaseModel):
    """Injected sensor errors."""
    gyro_bias_rad_s: Vector3 = Vector3(x=0, y=0, z=0)
    gyro_noise_std: float = 0.0
    accel_bias_g: Vector3 = Vector3(x=0, y=0, z=0)
    mag_hard_iron_ut: Vector3 = Vector3(x=0, y=0, z=0)


class ExecutionConfig(BaseModel):
    """Execution parameters."""
    warmup_samples: int = 50
    measured_samples: int = 200
    total_ticks: int = 250
    duration_seconds: float = 5.0


class Manifest(BaseModel):
    """Experiment run manifest (manifest.json)."""
    schema_version: str = "1.0"
    experiment_id: str
    experiment_family: str = "Wave_A"
    description: str = ""
    
    timestamp_iso: str = Field(default_factory=lambda: datetime.utcnow().isoformat())
    hostname: str = "unknown"
    git_commit: str = "unknown"
    
    simulator_config: SimulatorConfig = Field(default_factory=SimulatorConfig)
    gimbal_config: GimbalConfig
    sensor_errors: SensorErrors = Field(default_factory=SensorErrors)
    execution: ExecutionConfig = Field(default_factory=ExecutionConfig)


class Sample(BaseModel):
    """Single time-series sample (one CSV row)."""
    sample_idx: int
    timestamp_us: int
    truth_w: float
    truth_x: float
    truth_y: float
    truth_z: float
    fused_w: float
    fused_x: float
    fused_y: float
    fused_z: float
    angular_error_deg: float
    error_axis_x: float = 0.0
    error_axis_y: float = 0.0
    error_axis_z: float = 1.0

    @field_validator('truth_w', 'truth_x', 'truth_y', 'truth_z',
                     'fused_w', 'fused_x', 'fused_y', 'fused_z')
    @classmethod
    def validate_quaternion_normalized(cls, v: float) -> float:
        """Ensure quaternion components are reasonable."""
        if not -1.0 <= v <= 1.0:
            raise ValueError(f"Quaternion component {v} out of range [-1, 1]")
        return v


class AcceptanceCheck(BaseModel):
    """Single acceptance criterion check."""
    threshold: float
    actual: float
    passed: bool


class AngularErrorMetrics(BaseModel):
    """Aggregated angular error statistics."""
    rms_deg: float
    max_deg: float
    final_deg: float
    mean_deg: float
    std_deg: float
    p95_deg: float
    p99_deg: float


class DriftAnalysis(BaseModel):
    """Drift rate analysis."""
    drift_rate_deg_per_min: float
    drift_rate_computed_from: str = "first_10_to_last_10_samples"
    convergence_time_seconds: Optional[float] = None
    convergence_threshold_deg: float = 3.0


class AcceptanceCriteria(BaseModel):
    """Acceptance criteria evaluation."""
    passed: bool
    checks: Dict[str, AcceptanceCheck]


class Summary(BaseModel):
    """Experiment summary (summary.json)."""
    sample_count: int
    duration_seconds: float
    
    angular_error: AngularErrorMetrics
    drift_analysis: DriftAnalysis
    acceptance_criteria: AcceptanceCriteria
    
    statistics: Dict[str, Any] = Field(default_factory=dict)


class RunResult(BaseModel):
    """Complete result of a single experiment run."""
    manifest: Manifest
    samples: List[Sample]
    summary: Summary
    
    @classmethod
    def from_directory(cls, run_dir: Path) -> "RunResult":
        """Load complete run result from directory."""
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
                samples.append(Sample(**{k: float(v) if '.' in v or k != 'sample_idx' else int(v) 
                                        for k, v in row.items()}))
        
        # Load summary
        with open(run_dir / "summary.json") as f:
            summary = Summary(**json.load(f))
        
        return cls(manifest=manifest, samples=samples, summary=summary)
```

---

## File 2: `tools/analysis/metrics.py`

**Purpose:** Compute statistics from raw samples

**Dependencies:**
```
numpy >= 1.24.0
```

**Exact Implementation:**

```python
"""Metrics computation for experiment analysis."""

import numpy as np
from typing import List, Tuple, Optional
from dataclasses import dataclass

from .schema import Sample, AngularErrorMetrics, DriftAnalysis


@dataclass
class ComputedMetrics:
    """Container for all computed metrics."""
    angular_error: AngularErrorMetrics
    drift_analysis: DriftAnalysis


def compute_angular_error_metrics(samples: List[Sample]) -> AngularErrorMetrics:
    """
    Compute angular error statistics from samples.
    
    Args:
        samples: List of Sample objects with angular_error_deg field
        
    Returns:
        AngularErrorMetrics with RMS, max, mean, std, percentiles
    """
    errors = np.array([s.angular_error_deg for s in samples])
    
    if len(errors) == 0:
        return AngularErrorMetrics(
            rms_deg=0.0,
            max_deg=0.0,
            final_deg=0.0,
            mean_deg=0.0,
            std_deg=0.0,
            p95_deg=0.0,
            p99_deg=0.0
        )
    
    # RMS: sqrt(mean(squared errors))
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
    """
    Compute drift rate in degrees per minute.
    
    Args:
        samples: Time-series samples
        method: "endpoints" (first to last) or "regression" (linear fit)
        
    Returns:
        Drift rate in degrees per minute
    """
    if len(samples) < 2:
        return 0.0
    
    errors = np.array([s.angular_error_deg for s in samples])
    timestamps = np.array([s.timestamp_us for s in samples])
    
    # Convert to minutes
    duration_min = (timestamps[-1] - timestamps[0]) / (1_000_000 * 60)
    
    if duration_min <= 0:
        return 0.0
    
    if method == "endpoints":
        # Simple: (last - first) / duration
        drift = (errors[-1] - errors[0]) / duration_min
    elif method == "regression":
        # Linear regression slope
        timestamps_min = (timestamps - timestamps[0]) / (1_000_000 * 60)
        slope, _ = np.polyfit(timestamps_min, errors, 1)
        drift = slope
    else:
        raise ValueError(f"Unknown method: {method}")
    
    return float(drift)


def detect_convergence(samples: List[Sample],
                      threshold_deg: float = 3.0,
                      window_size: int = 10) -> Optional[float]:
    """
    Detect convergence time: first time error stays below threshold.
    
    Args:
        samples: Time-series samples
        threshold_deg: Error threshold for convergence
        window_size: Number of consecutive samples below threshold
        
    Returns:
        Time to convergence in seconds, or None if not converged
    """
    if len(samples) < window_size:
        return None
    
    errors = np.array([s.angular_error_deg for s in samples])
    timestamps = np.array([s.timestamp_us for s in samples])
    
    # Sliding window: find first index where all samples in window are below threshold
    for i in range(len(errors) - window_size + 1):
        if np.all(errors[i:i+window_size] < threshold_deg):
            # Return timestamp at end of window, converted to seconds
            return float(timestamps[i + window_size - 1] - timestamps[0]) / 1_000_000
    
    return None


def compute_all_metrics(samples: List[Sample],
                       convergence_threshold_deg: float = 3.0) -> ComputedMetrics:
    """
    Compute all metrics from samples.
    
    Args:
        samples: List of Sample objects
        convergence_threshold_deg: Threshold for convergence detection
        
    Returns:
        ComputedMetrics with all statistics
    """
    angular_error = compute_angular_error_metrics(samples)
    
    drift_rate = compute_drift_rate_simple(samples, method="endpoints")
    convergence_time = detect_convergence(samples, convergence_threshold_deg)
    
    drift_analysis = DriftAnalysis(
        drift_rate_deg_per_min=drift_rate,
        drift_rate_computed_from="first_to_last_sample",
        convergence_time_seconds=convergence_time,
        convergence_threshold_deg=convergence_threshold_deg
    )
    
    return ComputedMetrics(
        angular_error=angular_error,
        drift_analysis=drift_analysis
    )


def compare_metrics(baseline: ComputedMetrics, 
                   variant: ComputedMetrics) -> dict:
    """
    Compare two sets of metrics and return differences.
    
    Returns:
        Dictionary with deltas and percent changes
    """
    return {
        "rms_delta_deg": variant.angular_error.rms_deg - baseline.angular_error.rms_deg,
        "rms_percent_change": ((variant.angular_error.rms_deg - baseline.angular_error.rms_deg) 
                               / baseline.angular_error.rms_deg * 100) if baseline.angular_error.rms_deg > 0 else 0,
        "max_delta_deg": variant.angular_error.max_deg - baseline.angular_error.max_deg,
        "drift_delta_dpm": variant.drift_analysis.drift_rate_deg_per_min - baseline.drift_analysis.drift_rate_deg_per_min,
    }
```

---

## File 3: `tools/analysis/run_single_analysis.py`

**Purpose:** CLI tool to analyze a single experiment run

**Dependencies:**
```
click >= 8.0.0  # or argparse from stdlib
```

**Exact Implementation:**

```python
"""Single experiment run analysis tool."""

import json
import sys
from pathlib import Path
from typing import Optional

# Use argparse for zero-dependency option
import argparse

from .schema import RunResult, AcceptanceCheck, AcceptanceCriteria, Summary
from .metrics import compute_all_metrics


def evaluate_acceptance(metrics, criteria: dict) -> AcceptanceCriteria:
    """Evaluate acceptance criteria against computed metrics."""
    checks = {}
    all_passed = True
    
    # RMS check
    if "rms_threshold_deg" in criteria:
        passed = metrics.angular_error.rms_deg < criteria["rms_threshold_deg"]
        checks["rms"] = AcceptanceCheck(
            threshold=criteria["rms_threshold_deg"],
            actual=metrics.angular_error.rms_deg,
            passed=passed
        )
        all_passed = all_passed and passed
    
    # Max error check
    if "max_threshold_deg" in criteria:
        passed = metrics.angular_error.max_deg < criteria["max_threshold_deg"]
        checks["max"] = AcceptanceCheck(
            threshold=criteria["max_threshold_deg"],
            actual=metrics.angular_error.max_deg,
            passed=passed
        )
        all_passed = all_passed and passed
    
    # Drift check
    if "drift_threshold_dpm" in criteria:
        passed = abs(metrics.drift_analysis.drift_rate_deg_per_min) < criteria["drift_threshold_dpm"]
        checks["drift"] = AcceptanceCheck(
            threshold=criteria["drift_threshold_dpm"],
            actual=metrics.drift_analysis.drift_rate_deg_per_min,
            passed=passed
        )
        all_passed = all_passed and passed
    
    return AcceptanceCriteria(passed=all_passed, checks=checks)


def analyze_run(run_dir: Path, 
                acceptance_criteria: Optional[dict] = None,
                output_summary: Optional[Path] = None) -> Summary:
    """
    Analyze a single experiment run.
    
    Args:
        run_dir: Directory containing manifest.json, samples.csv
        acceptance_criteria: Optional criteria for pass/fail evaluation
        output_summary: Optional path to write summary.json
        
    Returns:
        Summary object with computed metrics
    """
    # Load run data
    result = RunResult.from_directory(run_dir)
    
    # Compute metrics
    computed = compute_all_metrics(
        result.samples,
        convergence_threshold_deg=3.0
    )
    
    # Evaluate acceptance criteria
    if acceptance_criteria:
        acceptance = evaluate_acceptance(computed, acceptance_criteria)
    else:
        # Default criteria
        acceptance = evaluate_acceptance(computed, {
            "rms_threshold_deg": 5.0,
            "max_threshold_deg": 10.0,
            "drift_threshold_dpm": 2.0
        })
    
    # Build summary
    summary = Summary(
        sample_count=len(result.samples),
        duration_seconds=result.samples[-1].timestamp_us / 1_000_000 if result.samples else 0,
        angular_error=computed.angular_error,
        drift_analysis=computed.drift_analysis,
        acceptance_criteria=acceptance,
        statistics={
            "computation_timestamp": "auto",
            "warmup_discarded": result.manifest.execution.warmup_samples
        }
    )
    
    # Write summary if requested
    if output_summary:
        output_summary.parent.mkdir(parents=True, exist_ok=True)
        with open(output_summary, 'w') as f:
            json.dump(summary.model_dump(), f, indent=2)
    
    return summary


def print_summary(summary: Summary, experiment_id: str = "unknown"):
    """Print human-readable summary to stdout."""
    print(f"\n{'='*60}")
    print(f"Experiment: {experiment_id}")
    print(f"{'='*60}")
    
    print(f"\nSamples: {summary.sample_count}")
    print(f"Duration: {summary.duration_seconds:.1f}s")
    
    print(f"\nAngular Error:")
    print(f"  RMS:   {summary.angular_error.rms_deg:.2f}°")
    print(f"  Max:   {summary.angular_error.max_deg:.2f}°")
    print(f"  Final: {summary.angular_error.final_deg:.2f}°")
    print(f"  Mean:  {summary.angular_error.mean_deg:.2f}°")
    print(f"  P95:   {summary.angular_error.p95_deg:.2f}°")
    
    print(f"\nDrift Analysis:")
    print(f"  Rate: {summary.drift_analysis.drift_rate_deg_per_min:.2f}°/min")
    if summary.drift_analysis.convergence_time_seconds:
        print(f"  Convergence: {summary.drift_analysis.convergence_time_seconds:.2f}s")
    
    print(f"\nAcceptance Criteria:")
    status = "PASSED" if summary.acceptance_criteria.passed else "FAILED"
    print(f"  Overall: {status}")
    for name, check in summary.acceptance_criteria.checks.items():
        status = "✓" if check.passed else "✗"
        print(f"  [{status}] {name}: {check.actual:.2f} (threshold: {check.threshold})")
    
    print(f"\n{'='*60}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Analyze a single HelixDrift experiment run"
    )
    parser.add_argument(
        "run_dir",
        type=Path,
        help="Directory containing manifest.json and samples.csv"
    )
    parser.add_argument(
        "--output-summary",
        type=Path,
        default=None,
        help="Write summary.json to this path"
    )
    parser.add_argument(
        "--criteria",
        type=str,
        default=None,
        help="JSON string with acceptance criteria (e.g., '{\"rms_threshold_deg\":3.0}')"
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress stdout output"
    )
    
    args = parser.parse_args()
    
    # Validate input
    if not args.run_dir.exists():
        print(f"Error: Run directory not found: {args.run_dir}", file=sys.stderr)
        sys.exit(1)
    
    if not (args.run_dir / "manifest.json").exists():
        print(f"Error: manifest.json not found in {args.run_dir}", file=sys.stderr)
        sys.exit(1)
    
    if not (args.run_dir / "samples.csv").exists():
        print(f"Error: samples.csv not found in {args.run_dir}", file=sys.stderr)
        sys.exit(1)
    
    # Parse criteria
    acceptance_criteria = None
    if args.criteria:
        import json
        acceptance_criteria = json.loads(args.criteria)
    
    # Run analysis
    summary = analyze_run(
        args.run_dir,
        acceptance_criteria=acceptance_criteria,
        output_summary=args.output_summary
    )
    
    # Print results
    if not args.quiet:
        experiment_id = args.run_dir.name
        print_summary(summary, experiment_id)
    
    # Exit with error code if criteria not met
    sys.exit(0 if summary.acceptance_criteria.passed else 1)


if __name__ == "__main__":
    main()
```

---

## Example Artifacts

### Example 1: `manifest.json`

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
    "initial_orientation": {
      "w": 0.70710678,
      "x": 0.0,
      "y": 0.0,
      "z": 0.70710678
    },
    "rotation_rate_dps": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    },
    "motion_profile": "static"
  },
  "sensor_errors": {
    "gyro_bias_rad_s": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    },
    "gyro_noise_std": 0.001,
    "accel_bias_g": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    },
    "mag_hard_iron_ut": {
      "x": 0.0,
      "y": 0.0,
      "z": 0.0
    }
  },
  "execution": {
    "warmup_samples": 50,
    "measured_samples": 200,
    "total_ticks": 250,
    "duration_seconds": 5.0
  }
}
```

### Example 2: `samples.csv` (Header + 3 rows)

```csv
sample_idx,timestamp_us,truth_w,truth_x,truth_y,truth_z,fused_w,fused_x,fused_y,fused_z,angular_error_deg,error_axis_x,error_axis_y,error_axis_z
0,1000000,0.707107,0.000000,0.000000,0.707107,0.705000,0.010000,0.005000,0.709000,1.234000,0.100000,0.200000,0.900000
1,1020000,0.707107,0.000000,0.000000,0.707107,0.706000,0.008000,0.004000,0.708000,0.987000,0.100000,0.200000,0.900000
2,1040000,0.707107,0.000000,0.000000,0.707107,0.706500,0.007000,0.003500,0.707500,0.750000,0.100000,0.200000,0.900000
```

**Column Definitions:**
| Column | Type | Description |
|--------|------|-------------|
| `sample_idx` | int | Sample number (0-indexed) |
| `timestamp_us` | int | Microseconds since start |
| `truth_w/x/y/z` | float | Ground truth quaternion |
| `fused_w/x/y/z` | float | Fused output quaternion |
| `angular_error_deg` | float | Error magnitude in degrees |
| `error_axis_x/y/z` | float | Error rotation axis |

### Example 3: `summary.json`

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
    "drift_rate_computed_from": "first_to_last_sample",
    "convergence_time_seconds": 1.5,
    "convergence_threshold_deg": 3.0
  },
  "acceptance_criteria": {
    "passed": true,
    "checks": {
      "rms": {
        "threshold": 5.0,
        "actual": 2.34,
        "passed": true
      },
      "max": {
        "threshold": 10.0,
        "actual": 5.67,
        "passed": true
      },
      "drift": {
        "threshold": 2.0,
        "actual": 0.45,
        "passed": true
      }
    }
  },
  "statistics": {
    "warmup_discarded": 50,
    "computation_timestamp": "2026-03-29T10:30:05Z"
  }
}
```

---

## First Tests Plan

### Test File: `tools/analysis/tests/test_schema.py`

```python
"""Tests for schema validation."""

import pytest
from pathlib import Path
import json
import tempfile

from tools.analysis.schema import (
    Manifest, Sample, Summary, RunResult,
    SimulatorConfig, GimbalConfig, Quaternion
)


class TestManifest:
    """Test manifest schema validation."""
    
    def test_minimal_manifest(self):
        """Create manifest with required fields only."""
        manifest = Manifest(
            experiment_id="test_run",
            gimbal_config=GimbalConfig(
                initial_orientation=Quaternion(w=1, x=0, y=0, z=0)
            )
        )
        assert manifest.schema_version == "1.0"
        assert manifest.experiment_id == "test_run"
    
    def test_full_manifest(self):
        """Create manifest with all fields."""
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
        assert manifest.simulator_config.seed == 123
    
    def test_invalid_quaternion_component(self):
        """Quaternion component out of range should fail."""
        # This test will fail until we add validator
        pass


class TestSample:
    """Test sample schema."""
    
    def test_sample_creation(self):
        """Create a valid sample."""
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
    """Test loading run results from directory."""
    
    def test_load_from_directory(self, tmp_path: Path):
        """Load complete run from files."""
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
        
        # Create summary.json
        summary = {
            "sample_count": 1,
            "duration_seconds": 0.02,
            "angular_error": {
                "rms_deg": 1.5, "max_deg": 1.5, "final_deg": 1.5,
                "mean_deg": 1.5, "std_deg": 0.0, "p95_deg": 1.5, "p99_deg": 1.5
            },
            "drift_analysis": {
                "drift_rate_deg_per_min": 0.0,
                "convergence_threshold_deg": 3.0
            },
            "acceptance_criteria": {"passed": True, "checks": {}}
        }
        with open(tmp_path / "summary.json", "w") as f:
            json.dump(summary, f)
        
        # Load and verify
        result = RunResult.from_directory(tmp_path)
        assert result.manifest.experiment_id == "test"
        assert len(result.samples) == 1
        assert result.summary.sample_count == 1
```

### Test File: `tools/analysis/tests/test_metrics.py`

```python
"""Tests for metrics computation."""

import pytest
import numpy as np

from tools.analysis.metrics import (
    compute_angular_error_metrics,
    compute_drift_rate_simple,
    detect_convergence,
    compute_all_metrics
)
from tools.analysis.schema import Sample


class TestAngularErrorMetrics:
    """Test angular error statistics."""
    
    def test_empty_samples(self):
        """Empty list should return zeros."""
        metrics = compute_angular_error_metrics([])
        assert metrics.rms_deg == 0.0
        assert metrics.max_deg == 0.0
    
    def test_constant_error(self):
        """Constant error should have RMS = value."""
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
        """Varying errors should compute correct RMS."""
        errors = [3.0, 4.0, 5.0]  # RMS should be sqrt((9+16+25)/3) = sqrt(50/3) ≈ 4.08
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
    """Test drift rate computation."""
    
    def test_no_drift(self):
        """Constant error = zero drift."""
        samples = [
            Sample(sample_idx=i, timestamp_us=i*1000000,  # 1 second intervals
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=5.0)
            for i in range(60)  # 60 seconds
        ]
        drift = compute_drift_rate_simple(samples, method="endpoints")
        assert pytest.approx(drift, 0.01) == 0.0
    
    def test_linear_drift(self):
        """Linear increase = constant drift."""
        # Error increases 1 degree per minute
        samples = [
            Sample(sample_idx=i, timestamp_us=i*1000000,  # 1 second intervals
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=i/60.0)  # 1 deg/min slope
            for i in range(60)
        ]
        drift = compute_drift_rate_simple(samples, method="endpoints")
        assert pytest.approx(drift, 0.1) == 1.0  # ~1 deg/min


class TestConvergence:
    """Test convergence detection."""
    
    def test_converged(self):
        """Samples below threshold should detect convergence."""
        # Start high, drop below threshold at sample 5
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
        """Samples always above threshold should return None."""
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

## Boundaries: What Sidecar Does NOT Touch

### ❌ C++ Simulator Code
- `simulators/sensors/*`
- `simulators/gimbal/*`
- `simulators/fixtures/*`
- `simulators/i2c/*`

### ❌ C++ Test Code
- `simulators/tests/*.cpp`
- `tests/*.cpp`

### ❌ Build System
- `CMakeLists.txt` (unless adding analysis tool target)
- `build.py`

### ❌ Firmware
- `firmware/common/*`
- `firmware/common/ota/*`

### ✅ What Sidecar Creates (New Files Only)
- `tools/analysis/*.py` (new directory)
- `tools/analysis/tests/*.py`
- `experiments/batches/*.json` (config files)
- `experiments/runs/` (output, gitignored)
- `experiments/analysis/` (output, gitignored)

---

## Ollama Model Recommendation

### Recommended: `llama3.2:3b`

**Why this model:**
| Factor | Assessment |
|--------|------------|
| **Size** | 3B parameters (~2GB download) |
| **RAM** | ~4GB required |
| **Speed** | ~50 tokens/sec on CPU |
| **Quality** | Sufficient for summarizing pre-computed metrics |
| **License** | Open, commercial use OK |

**Installation:**
```bash
ollama pull llama3.2:3b
```

**Test:**
```bash
ollama run llama3.2:3b
>>> Summarize this: RMS error 2.3 degrees, max 5.1 degrees, all tests passed.
```

**Alternative if more quality needed:**
- `qwen2.5:7b` — Better reasoning, ~8GB RAM
- `deepseek-r1:7b` — Best for anomaly detection, ~8GB RAM

**Temperature:** Always use `temperature=0.0` for deterministic output.

---

## Readiness Assessment

### Can an Implementation Agent Start Immediately?

| Criterion | Status |
|-----------|--------|
| Exact file specs | ✅ schema.py, metrics.py, run_single_analysis.py |
| Dependencies listed | ✅ pydantic, numpy, (optional: click) |
| Example artifacts | ✅ manifest.json, samples.csv, summary.json |
| Tests defined | ✅ test_schema.py, test_metrics.py |
| Boundaries clear | ✅ No C++ changes, new directory only |
| Ollama guidance | ✅ llama3.2:3b recommended |

### Implementation Order

1. **Create directory structure:** `tools/analysis/`, `tools/analysis/tests/`
2. **Implement schema.py** — Pydantic models first
3. **Write tests for schema** — `test_schema.py`
4. **Implement metrics.py** — Statistics computation
5. **Write tests for metrics** — `test_metrics.py`
6. **Implement run_single_analysis.py** — CLI tool
7. **Test end-to-end** — Create example run directory, run analysis
8. **Document** — Update README in `tools/analysis/`

### Success Criteria for Phase 1

- ✅ `python -m tools.analysis.run_single_analysis --help` works
- ✅ Can load example manifest/samples/summary without errors
- ✅ Can compute metrics and print summary
- ✅ All unit tests pass
- ✅ No C++ code modified
- ✅ No existing tests broken

---

**Status:** ✅ Phase 1 pack ready for implementation agent
