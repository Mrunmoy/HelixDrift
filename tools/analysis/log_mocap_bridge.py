#!/usr/bin/env python3
import argparse
import collections
import sys
import time

import serial


def parse_kv_line(line: str) -> tuple[str, dict[str, str]] | None:
    line = line.strip()
    if not line:
        return None
    parts = line.split()
    kind = parts[0]
    fields: dict[str, str] = {}
    for token in parts[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        fields[key] = value
    return kind, fields


def main() -> int:
    parser = argparse.ArgumentParser(description="Read Helix mocap bridge lines from a serial port")
    parser.add_argument("port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--summary-seconds", type=float, default=2.0)
    args = parser.parse_args()

    counts: collections.Counter[str] = collections.Counter()
    last_seq: dict[str, int] = {}
    last_summary = time.monotonic()

    with serial.Serial(args.port, args.baud, timeout=1.0) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                now = time.monotonic()
                if now - last_summary >= args.summary_seconds:
                    print(f"SUMMARY counts={dict(counts)}", flush=True)
                    last_summary = now
                continue

            line = raw.decode("utf-8", errors="replace").rstrip()
            parsed = parse_kv_line(line)
            print(line, flush=True)
            if parsed is None:
                continue

            kind, fields = parsed
            counts[kind] += 1
            if kind == "FRAME":
                node = fields.get("node", "?")
                seq = int(fields.get("seq", "0"))
                prev = last_seq.get(node)
                if prev is not None and ((prev + 1) & 0xFF) != seq:
                    print(f"GAP node={node} prev={prev} seq={seq}", flush=True)
                last_seq[node] = seq

            now = time.monotonic()
            if now - last_summary >= args.summary_seconds:
                print(f"SUMMARY counts={dict(counts)}", flush=True)
                last_summary = now

    return 0


if __name__ == "__main__":
    sys.exit(main())
