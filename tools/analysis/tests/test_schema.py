"""Tests for schema validation."""

import pytest
from pathlib import Path
import json

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
