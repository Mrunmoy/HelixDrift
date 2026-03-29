"""Single experiment run analysis tool."""

import json
import sys
from pathlib import Path
from typing import Optional

# Use argparse for zero-dependency option
import argparse

from tools.analysis.schema import RunResult, AcceptanceCheck, AcceptanceCriteria, Summary
from tools.analysis.metrics import compute_all_metrics


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
    # Load run data (raw - manifest + samples only)
    result = RunResult.from_raw_directory(run_dir)
    
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
        help='JSON string with acceptance criteria (e.g., \'{"rms_threshold_deg":3.0}\')'
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
        sys.exit(2)
    
    if not (args.run_dir / "manifest.json").exists():
        print(f"Error: manifest.json not found in {args.run_dir}", file=sys.stderr)
        sys.exit(2)
    
    if not (args.run_dir / "samples.csv").exists():
        print(f"Error: samples.csv not found in {args.run_dir}", file=sys.stderr)
        sys.exit(2)
    
    # Parse criteria
    acceptance_criteria = None
    if args.criteria:
        try:
            acceptance_criteria = json.loads(args.criteria)
        except json.JSONDecodeError as e:
            print(f"Error: Invalid criteria JSON: {e}", file=sys.stderr)
            sys.exit(2)
    
    # Run analysis
    try:
        summary = analyze_run(
            args.run_dir,
            acceptance_criteria=acceptance_criteria,
            output_summary=args.output_summary
        )
    except Exception as e:
        print(f"Error during analysis: {e}", file=sys.stderr)
        sys.exit(2)
    
    # Print results
    if not args.quiet:
        experiment_id = args.run_dir.name
        print_summary(summary, experiment_id)
    
    # Exit with error code if criteria not met
    sys.exit(0 if summary.acceptance_criteria.passed else 1)


if __name__ == "__main__":
    main()
