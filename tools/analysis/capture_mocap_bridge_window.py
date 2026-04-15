#!/usr/bin/env python3
import argparse
import collections
import csv
import pathlib
import re
import statistics
import sys
import time

import serial


FRAME_RE = re.compile(
    r"^FRAME node=(\d+) seq=(\d+) node_us=(\d+) sync_us=(\d+) rx_us=(\d+) "
    r"yaw_cd=(-?\d+) pitch_cd=(-?\d+) roll_cd=(-?\d+) "
    r"x_mm=(-?\d+) y_mm=(-?\d+) z_mm=(-?\d+) gaps=(\d+)$"
)


def pct(sorted_values: list[int], p: float) -> int:
    idx = min(len(sorted_values) - 1, int(len(sorted_values) * p))
    return sorted_values[idx]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture a timed mocap bridge window from the dongle USB stream"
    )
    parser.add_argument("port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--settle-seconds", type=float, default=30.0)
    parser.add_argument("--sample-seconds", type=float, default=10.0)
    parser.add_argument("--expected-nodes", type=int, default=0)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--summary", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    csv_path = pathlib.Path(args.csv)
    summary_path = pathlib.Path(args.summary)
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.parent.mkdir(parents=True, exist_ok=True)

    counts: collections.Counter[int] = collections.Counter()
    gap_totals: collections.Counter[int] = collections.Counter()
    last_sync: dict[int, int] = {}
    sync_deltas: list[int] = []
    last_summary = ""

    with serial.Serial(args.port, args.baud, timeout=0.5) as ser:
        settle_end = time.time() + args.settle_seconds
        while time.time() < settle_end:
            ser.readline()

        start = time.time()
        with csv_path.open("w", newline="", encoding="utf-8") as csv_file:
            writer = csv.writer(csv_file)
            writer.writerow(
                [
                    "sample_offset_s",
                    "node",
                    "seq",
                    "node_us",
                    "sync_us",
                    "rx_us",
                    "yaw_cd",
                    "pitch_cd",
                    "roll_cd",
                    "x_mm",
                    "y_mm",
                    "z_mm",
                    "gaps",
                ]
            )

            while time.time() - start < args.sample_seconds:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                if line.startswith("SUMMARY role=central"):
                    last_summary = line
                    continue

                match = FRAME_RE.match(line)
                if not match:
                    continue

                node = int(match.group(1))
                seq = int(match.group(2))
                node_us = int(match.group(3))
                sync_us = int(match.group(4))
                rx_us = int(match.group(5))
                yaw_cd = int(match.group(6))
                pitch_cd = int(match.group(7))
                roll_cd = int(match.group(8))
                x_mm = int(match.group(9))
                y_mm = int(match.group(10))
                z_mm = int(match.group(11))
                gaps = int(match.group(12))

                counts[node] += 1
                gap_totals[node] += gaps
                last_sync[node] = sync_us
                if len(last_sync) >= 2:
                    sync_values = list(last_sync.values())
                    sync_deltas.append(max(sync_values) - min(sync_values))

                writer.writerow(
                    [
                        f"{time.time() - start:.6f}",
                        node,
                        seq,
                        node_us,
                        sync_us,
                        rx_us,
                        yaw_cd,
                        pitch_cd,
                        roll_cd,
                        x_mm,
                        y_mm,
                        z_mm,
                        gaps,
                    ]
                )

    lines = []
    lines.append(f"COUNTS {dict(counts)}")
    lines.append(f"GAPS {dict(gap_totals)}")
    for node in sorted(counts):
        rate_hz = counts[node] / max(args.sample_seconds, 1e-9)
        gap_per_1k = gap_totals[node] / max(counts[node], 1) * 1000.0
        lines.append(f"RATE node={node} hz={rate_hz:.2f} gap_per_1k={gap_per_1k:.2f}")
    lines.append(f"RATE combined_hz={(sum(counts.values()) / max(args.sample_seconds, 1e-9)):.2f}")
    if last_summary:
        lines.append(last_summary)
    if sync_deltas:
        sync_deltas.sort()
        lines.append(
            "SYNC_SPAN_US "
            f"min={sync_deltas[0]} "
            f"median={int(statistics.median(sync_deltas))} "
            f"p90={pct(sync_deltas, 0.90)} "
            f"p99={pct(sync_deltas, 0.99)} "
            f"max={sync_deltas[-1]}"
        )

    summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    if args.expected_nodes > 0 and len(counts) < args.expected_nodes:
        raise SystemExit(
            f"expected frames from at least {args.expected_nodes} nodes, got {len(counts)}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
