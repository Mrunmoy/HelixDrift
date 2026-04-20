#!/usr/bin/env python3
"""Read a capture_mocap_bridge_window.py CSV + report per-node largest gap.

Usage:
  analyse_gaps.py <csv> [--fault-at <sec>]
"""
import argparse
import csv
import sys
from collections import defaultdict


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--fault-at", type=float, default=None,
                    help="seconds-since-capture-start of the injected fault; report gap across it")
    args = ap.parse_args()

    # per-node list of (sample_offset_s, seq)
    frames: dict[int, list[tuple[float, int]]] = defaultdict(list)
    with open(args.csv, newline="") as f:
        r = csv.DictReader(f)
        for row in r:
            try:
                t = float(row["sample_offset_s"])
                node = int(row["node"])
                seq = int(row["seq"])
            except (KeyError, ValueError):
                continue
            frames[node].append((t, seq))

    print(f"{'node':>4}  {'count':>6}  {'first_t':>8}  {'last_t':>8}  "
          f"{'max_gap_s':>10}  {'max_gap_at':>10}  {'pre_rate':>8}  {'post_rate':>9}")
    for node in sorted(frames):
        fs = frames[node]
        count = len(fs)
        if count < 2:
            print(f"{node:>4}  {count:>6}")
            continue
        max_gap = 0.0
        max_gap_at = 0.0
        prev_t = fs[0][0]
        for (t, _) in fs[1:]:
            gap = t - prev_t
            if gap > max_gap:
                max_gap = gap
                max_gap_at = prev_t  # gap starts at prev_t
            prev_t = t
        first_t = fs[0][0]
        last_t = fs[-1][0]

        if args.fault_at is not None:
            pre = [f for f in fs if f[0] < args.fault_at]
            post = [f for f in fs if f[0] >= args.fault_at]
            pre_rate = len(pre) / args.fault_at if args.fault_at > 0 else 0.0
            post_span = (last_t - args.fault_at) if last_t > args.fault_at else 1e-9
            post_rate = len(post) / post_span
        else:
            pre_rate = post_rate = count / (last_t - first_t + 1e-9)

        print(f"{node:>4}  {count:>6}  {first_t:>8.2f}  {last_t:>8.2f}  "
              f"{max_gap:>10.3f}  {max_gap_at:>10.2f}  {pre_rate:>8.2f}  {post_rate:>9.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
