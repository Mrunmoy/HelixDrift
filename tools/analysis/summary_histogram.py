#!/usr/bin/env python3
"""Parse SUMMARY lines from Hub CDC output and aggregate the RF-sync
instrumentation histograms (ack_lat on central, anchor_age + offset_step
on node).

The Hub emits SUMMARY every ~25 heartbeats (~500 ms). The histograms in
firmware are absolute monotonic counts, not deltas — so to get bucket
distributions over a window, take last_SUMMARY minus first_SUMMARY in
the window and convert to percentages.

Usage:
    summary_histogram.py <file>               # parse saved capture
    summary_histogram.py --live <tty>          # tail live CDC

Each bucket is labeled: <2 ms, 2-10, 10-30, >=30.
"""
import argparse
import re
import sys
from pathlib import Path

ACK_LAT_RE = re.compile(r"ack_lat=(\d+)/(\d+)/(\d+)/(\d+)")
PEND_MAX_RE = re.compile(r"pend_max=(\d+)")
ACK_DROP_RE = re.compile(r"ack_drop=(\d+)")
ANCHOR_AGE_RE = re.compile(r"anchor_age=(\d+)/(\d+)/(\d+)/(\d+)")
OFFSET_STEP_RE = re.compile(r"offset_step=(\d+)/(\d+)/(\d+)/(\d+)")
NODE_ID_RE = re.compile(r"SUMMARY role=node id=(\d+)")

BUCKETS = ["<2ms", "2-10ms", "10-30ms", ">=30ms"]


def report_deltas(label, first, last):
    if first is None or last is None:
        print(f"  {label}: no data")
        return
    total_first = sum(first)
    total_last = sum(last)
    deltas = [l - f for l, f in zip(last, first)]
    delta_total = sum(deltas)
    print(f"  {label}: total_first={total_first}  total_last={total_last}  "
          f"delta_total={delta_total}")
    if delta_total > 0:
        for name, d in zip(BUCKETS, deltas):
            pct = 100.0 * d / delta_total
            bar = "#" * int(round(pct / 2))
            print(f"    {name:>8}: {d:>8}  {pct:>5.1f}% {bar}")


def process_lines(lines):
    """Walk lines of Hub CDC output. Track first/last SUMMARY by role.
    For node-role summaries, also track per-node_id first/last."""
    first_hub_ack_lat = None
    last_hub_ack_lat = None
    last_hub_pend_max = None   # monotonic max — last sample is the peak so far
    last_hub_ack_drop = None
    per_node_first = {}  # node_id → (anchor_age, offset_step)
    per_node_last = {}

    for raw in lines:
        line = raw.strip()
        if "SUMMARY role=central" in line:
            m = ACK_LAT_RE.search(line)
            if m:
                vals = [int(m.group(i)) for i in range(1, 5)]
                if first_hub_ack_lat is None:
                    first_hub_ack_lat = vals
                last_hub_ack_lat = vals
            m_pm = PEND_MAX_RE.search(line)
            if m_pm:
                last_hub_pend_max = int(m_pm.group(1))
            m_ad = ACK_DROP_RE.search(line)
            if m_ad:
                last_hub_ack_drop = int(m_ad.group(1))
        elif "SUMMARY role=node" in line:
            m_nid = NODE_ID_RE.search(line)
            if not m_nid:
                continue
            nid = int(m_nid.group(1))
            m_age = ANCHOR_AGE_RE.search(line)
            m_step = OFFSET_STEP_RE.search(line)
            age = [int(m_age.group(i)) for i in range(1, 5)] if m_age else None
            step = [int(m_step.group(i)) for i in range(1, 5)] if m_step else None
            if nid not in per_node_first:
                per_node_first[nid] = (age, step)
            per_node_last[nid] = (age, step)

    print("=== Hub (central) ack-TX latency distribution ===")
    report_deltas("ack_lat", first_hub_ack_lat, last_hub_ack_lat)
    if last_hub_pend_max is not None:
        print(f"  peak ESB ACK-payload FIFO depth: {last_hub_pend_max}")
    if last_hub_ack_drop is not None:
        print(f"  TX_SUCCESS with empty ring (should be 0): {last_hub_ack_drop}")

    print()
    print("=== Per-Tag (node) anchor-age distribution ===")
    for nid in sorted(per_node_first):
        age_f, _ = per_node_first[nid]
        age_l, _ = per_node_last[nid]
        if age_f is None or age_l is None:
            continue
        print(f"Tag node_id={nid}:")
        report_deltas("anchor_age", age_f, age_l)

    print()
    print("=== Per-Tag (node) offset-step distribution ===")
    for nid in sorted(per_node_first):
        _, step_f = per_node_first[nid]
        _, step_l = per_node_last[nid]
        if step_f is None or step_l is None:
            continue
        print(f"Tag node_id={nid}:")
        report_deltas("offset_step", step_f, step_l)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", nargs="?", help="capture file (omit with --live)")
    ap.add_argument("--live", metavar="TTY", help="tail live CDC instead of a file")
    ap.add_argument("--seconds", type=int, default=0,
                    help="with --live, stop after this many seconds (0 = wait for Ctrl-C)")
    args = ap.parse_args()

    if args.live:
        import time
        import serial  # type: ignore
        with serial.Serial(args.live, 115200, timeout=1.0) as ser:
            lines = []
            start = time.time()
            try:
                while True:
                    line = ser.readline().decode(errors="replace")
                    if line:
                        lines.append(line)
                    if args.seconds and (time.time() - start) > args.seconds:
                        break
            except KeyboardInterrupt:
                pass
            process_lines(lines)
        return 0

    if args.file is None:
        print("usage: summary_histogram.py <file> | --live <tty>", file=sys.stderr)
        return 2

    with open(args.file) as fp:
        process_lines(fp.readlines())
    return 0


if __name__ == "__main__":
    sys.exit(main())
