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
    rotation_rate_dps: Vector3 = Field(default_factory=lambda: Vector3(x=0, y=0, z=0))
    motion_profile: str = "static"


class SensorErrors(BaseModel):
    """Injected sensor errors."""
    gyro_bias_rad_s: Vector3 = Field(default_factory=lambda: Vector3(x=0, y=0, z=0))
    gyro_noise_std: float = 0.0
    accel_bias_g: Vector3 = Field(default_factory=lambda: Vector3(x=0, y=0, z=0))
    mag_hard_iron_ut: Vector3 = Field(default_factory=lambda: Vector3(x=0, y=0, z=0))


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
