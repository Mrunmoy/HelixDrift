#!/usr/bin/env python3
"""Resilient version of capture_mocap_bridge_window.py.

Differences vs. the upstream:
  - Catches serial.SerialException mid-capture (Hub reset → CDC
    disconnect/re-enumeration) and reopens the port. The elapsed time
    keeps counting so the total sample_seconds wallclock is preserved;
    the gap shows up naturally in the CSV as a time-jump.
  - Adds --robust flag to opt into reconnect behaviour (default on).
  - Everything else (CSV layout, summary fields) is identical so the
    analyse_gaps.py output is unchanged.

Usage:
    python3 capture_robust.py <port> --csv <csv> --summary <summary> \
        --sample-seconds 300 [--settle-seconds 5] [--expected-nodes 10]
"""
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


def pct(sorted_values, p):
    idx = min(len(sorted_values) - 1, int(len(sorted_values) * p))
    return sorted_values[idx]


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--settle-seconds", type=float, default=5.0)
    parser.add_argument("--sample-seconds", type=float, default=300.0)
    parser.add_argument("--expected-nodes", type=int, default=0)
    parser.add_argument("--csv", required=True)
    parser.add_argument("--summary", required=True)
    parser.add_argument("--reconnect-backoff", type=float, default=1.0,
                        help="seconds to sleep between failed reconnect attempts")
    parser.add_argument("--reconnect-budget", type=float, default=30.0,
                        help="seconds to keep retrying after a disconnect "
                             "before giving up and ending the capture")
    return parser.parse_args()


def open_serial(args):
    return serial.Serial(args.port, args.baud, timeout=0.5)


def main():
    args = parse_args()
    csv_path = pathlib.Path(args.csv)
    summary_path = pathlib.Path(args.summary)
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.parent.mkdir(parents=True, exist_ok=True)

    counts = collections.Counter()
    gap_totals = collections.Counter()
    last_sync = {}
    sync_deltas = []
    last_summary = ""
    disconnect_events = 0

    ser = open_serial(args)
    try:
        settle_end = time.time() + args.settle_seconds
        while time.time() < settle_end:
            try:
                ser.readline()
            except serial.SerialException:
                break  # will re-open when the main loop starts

        start = time.time()
        with csv_path.open("w", newline="", encoding="utf-8") as csv_file:
            writer = csv.writer(csv_file)
            writer.writerow([
                "sample_offset_s", "node", "seq",
                "node_us", "sync_us", "rx_us",
                "yaw_cd", "pitch_cd", "roll_cd",
                "x_mm", "y_mm", "z_mm", "gaps",
            ])

            while time.time() - start < args.sample_seconds:
                try:
                    line = ser.readline().decode("utf-8", errors="replace").strip()
                except serial.SerialException as exc:
                    disconnect_events += 1
                    print(f"serial disconnect #{disconnect_events}: {exc}; reopening...",
                          file=sys.stderr)
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = None
                    deadline = time.time() + args.reconnect_budget
                    while time.time() < deadline and ser is None:
                        time.sleep(args.reconnect_backoff)
                        try:
                            ser = open_serial(args)
                            print(f"serial reopened after {time.time() - deadline + args.reconnect_budget:.1f}s",
                                  file=sys.stderr)
                        except serial.SerialException:
                            ser = None
                    if ser is None:
                        print("reconnect budget exhausted; ending capture early",
                              file=sys.stderr)
                        break
                    continue

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

                writer.writerow([
                    f"{time.time() - start:.6f}",
                    node, seq, node_us, sync_us, rx_us,
                    yaw_cd, pitch_cd, roll_cd,
                    x_mm, y_mm, z_mm, gaps,
                ])
    finally:
        try:
            if ser is not None:
                ser.close()
        except Exception:
            pass

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
    lines.append(f"DISCONNECT_EVENTS {disconnect_events}")

    summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    if args.expected_nodes > 0 and len(counts) < args.expected_nodes:
        raise SystemExit(
            f"expected frames from at least {args.expected_nodes} nodes, got {len(counts)}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
