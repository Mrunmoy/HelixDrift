"""Smoke tests for run plotting."""

import json
from pathlib import Path

from tools.analysis.plot_single_run import plot_run


def write_fixture(run_dir: Path) -> None:
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "manifest.json").write_text(
        json.dumps(
            {
                "experiment_id": "plot_fixture",
                "gimbal_config": {
                    "initial_orientation": {"w": 1, "x": 0, "y": 0, "z": 0},
                },
            }
        )
    )
    (run_dir / "samples.csv").write_text(
        "\n".join(
            [
                "sample_idx,timestamp_us,truth_w,truth_x,truth_y,truth_z,fused_w,fused_x,fused_y,fused_z,angular_error_deg",
                "0,0,1,0,0,0,1,0,0,0,0.0",
                "1,20000,0.99,0,0,0.1,0.98,0,0,0.2,1.5",
                "2,40000,0.97,0,0,0.25,0.95,0,0,0.3,3.0",
            ]
        )
    )


def test_plot_run_writes_pngs(tmp_path: Path):
    run_dir = tmp_path / "run"
    out_dir = tmp_path / "plots"
    write_fixture(run_dir)

    written = plot_run(run_dir, out_dir)

    assert (out_dir / "angular_error.png").exists()
    assert (out_dir / "quaternion_components.png").exists()
    assert len(written) == 2
