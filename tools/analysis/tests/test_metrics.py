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


class TestComputeAllMetrics:
    """Test the all-metrics convenience function."""
    
    def test_compute_all_metrics(self):
        """Compute all metrics from samples."""
        samples = [
            Sample(sample_idx=i, timestamp_us=i*20000,
                   truth_w=1, truth_x=0, truth_y=0, truth_z=0,
                   fused_w=1, fused_x=0, fused_y=0, fused_z=0,
                   angular_error_deg=float(i))
            for i in range(10)
        ]
        computed = compute_all_metrics(samples, convergence_threshold_deg=5.0)
        
        assert computed.angular_error.rms_deg > 0
        assert computed.angular_error.max_deg == 9.0
        assert computed.drift_analysis.drift_rate_deg_per_min is not None
