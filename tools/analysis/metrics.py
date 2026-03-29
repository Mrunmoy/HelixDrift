"""Metrics computation for experiment analysis."""

import numpy as np
from typing import List, Tuple, Optional
from dataclasses import dataclass

from tools.analysis.schema import Sample, AngularErrorMetrics, DriftAnalysis


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
