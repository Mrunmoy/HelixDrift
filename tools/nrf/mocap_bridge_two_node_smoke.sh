#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REMOTE_HOST="${1:?usage: $0 <remote-host> <remote-repo> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [remote-node-id] [local-node-id]}"
REMOTE_REPO="${2:?usage: $0 <remote-host> <remote-repo> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [remote-node-id] [local-node-id]}"
REMOTE_JLINK_SERIAL="${3:-123456}"
DEVICE="${4:-NRF52840_XXAA}"
LOCAL_JLINK_SERIAL="${5:-69656876}"
DONGLE_TTY="${6:-/dev/ttyACM3}"
SAMPLE_SECONDS="${7:-10}"
REMOTE_NODE_ID="${8:-2}"
LOCAL_NODE_ID="${9:-1}"

NCS_VERSION="${NCS_VERSION:-v3.2.4}"
WORKSPACE_DIR="${NCS_WORKSPACE_DIR:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"
LOCAL_NODE_HEX="${WORKSPACE_DIR}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node${LOCAL_NODE_ID}/nrf52840-mocap-bridge/zephyr/zephyr.hex"
CENTRAL_HEX="${WORKSPACE_DIR}/build-helix-nrf52840dongle-nrf52840-bare-mocap-central-node1/nrf52840-mocap-bridge/zephyr/zephyr.hex"

build_local() {
  "${REPO_ROOT}/tools/nrf/build_mocap_bridge.sh" central nrf52840dongle/nrf52840/bare 1
  "${REPO_ROOT}/tools/nrf/build_mocap_bridge.sh" node promicro_nrf52840/nrf52840/uf2 "${LOCAL_NODE_ID}"
}

flash_local() {
  "${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" "${CENTRAL_HEX}" "${LOCAL_JLINK_SERIAL}" "${DEVICE}" 0
  "${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" "${LOCAL_NODE_HEX}" 123456 "${DEVICE}" 0
}

build_remote() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ./tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 '${REMOTE_NODE_ID}'"
}

flash_remote() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ./tools/nrf/flash_hex_jlink.sh '.deps/ncs/${NCS_VERSION}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node${REMOTE_NODE_ID}/nrf52840-mocap-bridge/zephyr/zephyr.hex' '${REMOTE_JLINK_SERIAL}' '${DEVICE}' 0"
}

sample_bridge() {
  python3 - "${DONGLE_TTY}" "${SAMPLE_SECONDS}" <<'PY'
import collections
import re
import serial
import statistics
import sys
import time

port = sys.argv[1]
sample_seconds = float(sys.argv[2])
ser = serial.Serial(port, 115200, timeout=0.5)

# Drain initial chatter after coordinated flash/reset.
# The three-device lane needs a longer settle than the older two-ProPico smoke.
settle_end = time.time() + 30.0
while time.time() < settle_end:
    ser.readline()

counts = collections.Counter()
gaps = collections.Counter()
last_summary = None
last_sync = {}
sync_deltas = []
start = time.time()
while time.time() - start < sample_seconds:
    line = ser.readline().decode("utf-8", errors="replace").strip()
    if line.startswith("SUMMARY role=central"):
        last_summary = line
    m = re.match(r"^FRAME node=(\d+) seq=(\d+) node_us=(\d+) sync_us=(\d+) rx_us=(\d+) .* gaps=(\d+)$", line)
    if not m:
        continue
    node = int(m.group(1))
    sync_us = int(m.group(4))
    gap = int(m.group(6))
    counts[node] += 1
    gaps[node] += gap
    last_sync[node] = sync_us
    if len(last_sync) >= 2:
        nodes = sorted(last_sync)
        sync_deltas.append(abs(last_sync[nodes[0]] - last_sync[nodes[1]]))

ser.close()

wall = sample_seconds
print(f"COUNTS {dict(counts)}")
print(f"GAPS {dict(gaps)}")
for node in sorted(counts):
    print(f"RATE node={node} hz={counts[node]/wall:.2f} gap_per_1k={gaps[node]/max(counts[node], 1)*1000:.2f}")
print(f"RATE combined_hz={(sum(counts.values())/wall):.2f}")
if last_summary:
    print(last_summary)
if sync_deltas:
    sync_deltas.sort()
    def pct(p: float) -> int:
        idx = min(len(sync_deltas) - 1, int(len(sync_deltas) * p))
        return sync_deltas[idx]
    print(
        "SYNC_DELTA_US "
        f"min={sync_deltas[0]} "
        f"median={int(statistics.median(sync_deltas))} "
        f"p90={pct(0.90)} "
        f"p99={pct(0.99)} "
        f"max={sync_deltas[-1]}"
    )

if len(counts) < 2:
    raise SystemExit("expected frames from at least 2 nodes")

for node, c in counts.items():
    if c < sample_seconds * 40:
        raise SystemExit(f"node {node} rate too low: only {c} frames in {sample_seconds}s")
PY
}

build_local
build_remote
flash_local
flash_remote
sample_bridge
