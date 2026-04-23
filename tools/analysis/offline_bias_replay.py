#!/usr/bin/env python3
"""Offline replay: validate the drift-hypothesis on an existing capture
by applying PC-side bias corrections and measuring cross-Tag span
improvement.

Per Codex round-10 recommendation (docs/reviews/codex_2026-04-23_round10.md):
if cross-Tag span p99 collapses only with ROLLING bias correction
(not a fixed one), we've confirmed drift/wander — validating the
PC-side bias-tracker architecture BEFORE touching firmware.

Three modes:
  raw          - no correction, baseline (same as sync_error_analysis.py)
  fixed        - subtract each Tag's overall mean bias
  rolling-N    - subtract each Tag's rolling-N-second median-vs-fleet bias

Usage:
    offline_bias_replay.py <csv> [--exclude N [N ...]] [--rolling-window-s S]
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict


def load(csv_path, exclude):
    rows = []
    with open(csv_path) as fp:
        rdr = csv.DictReader(fp)
        for row in rdr:
            try:
                node = int(row["node"])
                if node in exclude:
                    continue
                sync_us = int(row["sync_us"])
                rx_us = int(row["rx_us"])
            except (KeyError, ValueError):
                continue
            rows.append((node, sync_us, rx_us))
    return rows


def span_stats(rows, bin_us=50_000, min_tags=5):
    """Per-bin cross-Tag span p50/p90/p99/max from a list of (node, sync_us, rx_us)."""
    bins = defaultdict(lambda: defaultdict(list))
    for node, sync_us, rx_us in rows:
        bins[rx_us // bin_us][node].append(sync_us)
    spans = []
    for tags in bins.values():
        if len(tags) < min_tags:
            continue
        medians = [statistics.median(vs) for vs in tags.values()]
        spans.append(max(medians) - min(medians))
    if not spans:
        return None
    spans.sort()
    n = len(spans)
    return {
        "bins": n,
        "p50": spans[n // 2] / 1000.0,
        "p90": spans[int(n * 0.9)] / 1000.0,
        "p99": spans[int(n * 0.99)] / 1000.0,
        "max": spans[-1] / 1000.0,
    }


def per_tag_err_stats(rows):
    """Per-Tag |err| = |sync_us - rx_us|, returning mean + p99 per Tag."""
    errs = defaultdict(list)
    for node, sync_us, rx_us in rows:
        errs[node].append(sync_us - rx_us)
    out = {}
    for node, err_list in sorted(errs.items()):
        err_list.sort()
        n = len(err_list)
        abs_err = sorted(abs(e) for e in err_list)
        out[node] = {
            "n": n,
            "mean": statistics.mean(err_list) / 1000.0,  # ms
            "abs_p50": abs_err[n // 2] / 1000.0,
            "abs_p99": abs_err[int(n * 0.99)] / 1000.0,
        }
    return out


def apply_fixed_bias(rows):
    """Subtract each Tag's overall mean bias (sync_us - rx_us)."""
    sums = defaultdict(lambda: [0, 0])
    for node, sync_us, rx_us in rows:
        sums[node][0] += sync_us - rx_us
        sums[node][1] += 1
    bias = {node: s / n for node, (s, n) in sums.items()}
    return [(node, sync_us - int(bias[node]), rx_us) for node, sync_us, rx_us in rows]


def apply_rolling_bias(rows, window_us):
    """For each row, subtract Tag's rolling-window median err relative to
    the fleet median of all Tags in that window. Output rows retain the
    same (node, sync_us, rx_us) shape but with corrected sync_us."""
    # Sort by rx_us for streaming window
    rows_sorted = sorted(rows, key=lambda r: r[2])
    # For each row, compute Tag's rolling mean err over last window_us
    # of rx_us. Using a simple linear scan + deque per Tag.
    from collections import deque
    tag_windows = defaultdict(deque)  # node → deque of (rx_us, err)
    fleet_window = deque()  # all (node, rx_us, err)
    out = []
    for node, sync_us, rx_us in rows_sorted:
        err = sync_us - rx_us
        # Evict stale entries from all windows
        cutoff = rx_us - window_us
        dq = tag_windows[node]
        while dq and dq[0][0] < cutoff:
            dq.popleft()
        while fleet_window and fleet_window[0][1] < cutoff:
            fleet_window.popleft()
        # Append current
        dq.append((rx_us, err))
        fleet_window.append((node, rx_us, err))
        # Compute Tag's rolling median err vs fleet rolling median
        if len(dq) >= 3 and len(fleet_window) >= 10:
            tag_med = statistics.median(e for _, e in dq)
            fleet_med = statistics.median(e for _, _, e in fleet_window)
            rel_bias = tag_med - fleet_med
            corrected = sync_us - rel_bias
            out.append((node, int(corrected), rx_us))
        else:
            # Not enough data yet, pass through
            out.append((node, sync_us, rx_us))
    return out


def report(label, rows):
    span = span_stats(rows)
    per_tag = per_tag_err_stats(rows)
    print(f"\n=== {label} ===")
    if span:
        print(f"  Cross-Tag span p50/p90/p99/max:"
              f"  {span['p50']:.1f} / {span['p90']:.1f} / "
              f"{span['p99']:.1f} / {span['max']:.1f} ms ({span['bins']} bins)")
    print(f"  Per-Tag |err| p99 range: "
          f"{min(t['abs_p99'] for t in per_tag.values()):.1f} - "
          f"{max(t['abs_p99'] for t in per_tag.values()):.1f} ms")
    print(f"  Per-Tag mean bias range: "
          f"{min(t['mean'] for t in per_tag.values()):+.2f} .. "
          f"{max(t['mean'] for t in per_tag.values()):+.2f} ms")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("--exclude", type=int, nargs="+", default=[])
    ap.add_argument("--rolling-window-s", type=float, default=10.0)
    args = ap.parse_args()

    rows = load(args.csv, set(args.exclude))
    print(f"Loaded {len(rows)} rows from {args.csv} (excl {args.exclude})")

    report("raw (no correction)", rows)
    report("fixed bias (each Tag's overall mean subtracted)",
           apply_fixed_bias(rows))
    report(f"rolling-{args.rolling_window_s}s bias (Tag vs fleet median)",
           apply_rolling_bias(rows, int(args.rolling_window_s * 1e6)))


if __name__ == "__main__":
    main()
