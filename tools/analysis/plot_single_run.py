"""Plot a single HelixDrift run directory."""

from pathlib import Path
import argparse

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from tools.analysis.schema import RunResult


def plot_run(run_dir: Path, output_dir: Path | None = None) -> list[Path]:
    result = RunResult.from_raw_directory(run_dir)

    if output_dir is None:
        output_dir = run_dir / "plots"
    output_dir.mkdir(parents=True, exist_ok=True)

    timestamps_s = [sample.timestamp_us / 1_000_000.0 for sample in result.samples]
    errors_deg = [sample.angular_error_deg for sample in result.samples]

    error_fig, error_ax = plt.subplots(figsize=(8, 4))
    error_ax.plot(timestamps_s, errors_deg, label="Angular Error")
    error_ax.set_title("Angular Error Over Time")
    error_ax.set_xlabel("Time (s)")
    error_ax.set_ylabel("Error (deg)")
    error_ax.grid(True)
    error_ax.legend()
    error_path = output_dir / "angular_error.png"
    error_fig.tight_layout()
    error_fig.savefig(error_path)
    plt.close(error_fig)

    quat_fig, quat_axes = plt.subplots(4, 1, figsize=(8, 8), sharex=True)
    components = ("w", "x", "y", "z")
    for axis, component in zip(quat_axes, components):
        truth = [getattr(sample, f"truth_{component}") for sample in result.samples]
        fused = [getattr(sample, f"fused_{component}") for sample in result.samples]
        axis.plot(timestamps_s, truth, label=f"truth_{component}")
        axis.plot(timestamps_s, fused, label=f"fused_{component}", linestyle="--")
        axis.set_ylabel(component)
        axis.grid(True)
        axis.legend(loc="best")

    quat_axes[-1].set_xlabel("Time (s)")
    quat_fig.suptitle("Truth vs Fused Quaternion Components")
    quat_path = output_dir / "quaternion_components.png"
    quat_fig.tight_layout()
    quat_fig.savefig(quat_path)
    plt.close(quat_fig)

    return [error_path, quat_path]


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot a single HelixDrift run directory")
    parser.add_argument("run_dir", type=Path, help="Directory containing manifest.json and samples.csv")
    parser.add_argument("--output-dir", type=Path, default=None, help="Directory to write plots into")
    args = parser.parse_args()

    written = plot_run(args.run_dir, args.output_dir)
    for path in written:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
