#!/usr/bin/env python3
"""Analyse per-Tag sync error and cross-Tag sync span from a
capture_mocap_bridge_window.py CSV.

Cross-Tag span is computed in fixed wall-clock bins (based on Hub's
rx_us) instead of as a running max-min over last-seen-values. The
running metric gets pathologically biased when any Tag stops
transmitting — its sync_us stays stale and the reported span
explodes. The bin-based metric excludes stale Tags by construction.

Per-Tag sync error = frame.sync_us - frame.rx_us
  (frame.rx_us is Hub's local clock when it received the frame;
   frame.sync_us is Tag's synced_us = local - estimated_offset.
   For a perfectly-sync'd Tag these should be identical; a
   negative value means Tag thinks it's earlier than the Hub
   actually sees it. Observed systematic bias of ~-15 ms on the
   current wire protocol is a known artefact of ESB ACK-payload
   TIFS timing; see docs/RF_ROBUSTNESS_REPORT.md.)

Usage:
  sync_error_analysis.py <csv> [--bin-us N] [--min-tags-per-bin K]
                              [--exclude N [N ...]]

Exits 0 on success, 2 on bad args.
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict


def percentile(sorted_vals, p):
    if not sorted_vals:
        return 0
    return sorted_vals[min(len(sorted_vals) - 1, int(len(sorted_vals) * p))]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--bin-us", type=int, default=50_000,
                    help="wall-clock bin width in microseconds (default 50000 = 50 ms)")
    ap.add_argument("--min-tags-per-bin", type=int, default=5,
                    help="a bin is included in cross-Tag span stats only if this "
                         "many Tags reported in it (default 5 of 10)")
    ap.add_argument("--exclude", type=int, nargs="+", default=[],
                    help="node_ids to exclude from analysis (e.g. known-broken Tags)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    excluded = set(args.exclude)
    per_node_error = defaultdict(list)    # node -> list of (sync_us - rx_us)
    bins = defaultdict(dict)              # bin_index -> {node: sync_us}

    total_rows = 0
    with open(args.csv) as fp:
        reader = csv.DictReader(fp)
        for row in reader:
            try:
                node = int(row["node"])
                sync_us = int(row["sync_us"])
                rx_us = int(row["rx_us"])
            except (KeyError, ValueError):
                continue
            total_rows += 1
            if node in excluded:
                continue
            per_node_error[node].append(sync_us - rx_us)
            bin_idx = rx_us // args.bin_us
            bins[bin_idx][node] = sync_us

    if not per_node_error:
        print(f"no usable frames in {args.csv}", file=sys.stderr)
        return 2

    print(f"=== Sync-error analysis: {args.csv} ===")
    print(f"    {total_rows} rows read, {len(per_node_error)} Tags analysed"
          f"{' (excluded: ' + ','.join(map(str, sorted(excluded))) + ')' if excluded else ''}")
    print()

    # Per-Tag error stats
    print(f"{'node':>4}  {'n':>7}  {'mean_us':>9}  {'|err| p50':>9}  "
          f"{'|err| p90':>9}  {'|err| p99':>9}  {'|err| max':>10}")
    biases = []
    for node in sorted(per_node_error):
        vals = per_node_error[node]
        biases.append(statistics.mean(vals))
        abs_vals = sorted(abs(v) for v in vals)
        print(f"{node:>4}  {len(vals):>7}  "
              f"{statistics.mean(vals):>9.0f}  "
              f"{percentile(abs_vals, 0.50):>9}  "
              f"{percentile(abs_vals, 0.90):>9}  "
              f"{percentile(abs_vals, 0.99):>9}  "
              f"{abs_vals[-1]:>10}")
    print()
    print(f"fleet mean per-Tag bias: {statistics.mean(biases):>.0f} µs "
          f"({min(biases):.0f} .. {max(biases):.0f})")
    print()

    # Cross-Tag span
    spans = []
    for bin_idx, node_syncs in bins.items():
        if len(node_syncs) >= args.min_tags_per_bin:
            vs = list(node_syncs.values())
            spans.append(max(vs) - min(vs))
    spans.sort()
    print(f"=== Cross-Tag instantaneous span ({args.bin_us} µs bins, "
          f">= {args.min_tags_per_bin} Tags per bin) ===")
    if not spans:
        print(f"   (no bins met the {args.min_tags_per_bin}-Tag threshold — "
              "try lowering --min-tags-per-bin or widening --bin-us)")
    else:
        print(f"    bins={len(spans)}  "
              f"p50={percentile(spans, 0.50):>6}  "
              f"p90={percentile(spans, 0.90):>6}  "
              f"p99={percentile(spans, 0.99):>6}  "
              f"max={spans[-1]:>6} µs")

    return 0


if __name__ == "__main__":
    sys.exit(main())
