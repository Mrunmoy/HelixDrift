#!/usr/bin/env python3
"""Find cross-Tag span outlier events in a mocap bridge CSV.

For each 50ms wall-clock bin with >= 5 Tags reporting, compute the span
(max sync_us - min sync_us). Print bins where span > threshold, showing
which Tag is the outlier and its sync_us vs. the bin median.

Use case: debugging the "per-Tag |err| p99 is 10ms but fleet span p99
is 50ms" gap — find the specific events that drive the fat tail.

Usage:
    find_span_outliers.py <csv> [--threshold-ms N] [--max-events M]
                               [--exclude-nodes N [N ...]]
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--threshold-ms", type=float, default=30.0,
                    help="only report bins with span > this many ms (default 30)")
    ap.add_argument("--bin-us", type=int, default=50_000,
                    help="bin size in microseconds (default 50000 = 50ms)")
    ap.add_argument("--min-tags", type=int, default=5,
                    help="minimum Tags required in a bin to report (default 5)")
    ap.add_argument("--max-events", type=int, default=20,
                    help="max outlier events to print (default 20)")
    ap.add_argument("--exclude-nodes", type=int, nargs="+", default=[],
                    help="Tag IDs to exclude (e.g. --exclude-nodes 8 10)")
    args = ap.parse_args()

    # bin_us → {node_id: [sync_us, ...]}
    bins = defaultdict(lambda: defaultdict(list))

    with open(args.csv) as fp:
        rdr = csv.DictReader(fp)
        for row in rdr:
            try:
                node = int(row["node"])
                if node in args.exclude_nodes:
                    continue
                sync_us = int(row["sync_us"])
                rx_us = int(row["rx_us"])
            except (KeyError, ValueError):
                continue
            key = rx_us // args.bin_us
            bins[key][node].append(sync_us)

    # Compute span per bin
    outliers = []
    for bin_key, tags in bins.items():
        if len(tags) < args.min_tags:
            continue
        # use median sync_us per Tag in this bin
        medians = {nid: statistics.median(vs) for nid, vs in tags.items()}
        span = max(medians.values()) - min(medians.values())
        if span > args.threshold_ms * 1000:
            bin_median = statistics.median(medians.values())
            # Identify the outlier — Tag furthest from bin median
            worst = max(medians, key=lambda nid: abs(medians[nid] - bin_median))
            outliers.append({
                "rx_us": bin_key * args.bin_us,
                "span_ms": span / 1000.0,
                "n_tags": len(tags),
                "worst_node": worst,
                "worst_delta_ms": (medians[worst] - bin_median) / 1000.0,
                "medians": {nid: m / 1000.0 for nid, m in medians.items()},  # ms
            })

    outliers.sort(key=lambda e: -e["span_ms"])
    print(f"Found {len(outliers)} bins with span > {args.threshold_ms} ms")
    print(f"Top {min(args.max_events, len(outliers))} by span:\n")
    for ev in outliers[:args.max_events]:
        print(f"  rx_us={ev['rx_us']}  span={ev['span_ms']:.1f}ms  "
              f"n_tags={ev['n_tags']}  worst=node{ev['worst_node']} "
              f"({ev['worst_delta_ms']:+.1f}ms)")

    # Summary: which Tags appear most often as the worst?
    worst_count = defaultdict(int)
    for ev in outliers:
        worst_count[ev["worst_node"]] += 1
    print(f"\n=== Outlier frequency by Tag ===")
    for nid, cnt in sorted(worst_count.items(), key=lambda x: -x[1]):
        print(f"  node {nid:2d}: {cnt} outlier bins")


if __name__ == "__main__":
    main()
