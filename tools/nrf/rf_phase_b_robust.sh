#!/usr/bin/env bash
# Phase B with the robust capture tool (survives Hub reset).
# Resets Hub mid-capture so you can measure the dropout + rejoin.
set -u
SAMPLE_SEC="${1:-300}"
FAULT_AT_SEC="${2:-60}"
ARTDIR="${HELIX_RF_ARTIFACT_DIR:-/tmp/helix_tag_log}"
PREFIX="${3:-${ARTDIR}/rf_phaseb_robust_$(date +%Y%m%d_%H%M%S)}"
HUB_SN="${HELIX_HUB_JLINK_SN:-69656876}"
HUB_TTY="${HELIX_HUB_TTY:-/dev/ttyACM1}"

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CAPTURE_TOOL="${REPO_ROOT}/tools/analysis/capture_mocap_bridge_robust.py"

mkdir -p "$(dirname "$PREFIX")"
cd "$REPO_ROOT"

echo "=== Phase B (robust) — mid-stream Hub reset ==="
echo "Sample: ${SAMPLE_SEC} s, fault at T+${FAULT_AT_SEC} s"
echo "Artifact: ${PREFIX}"
echo "Capture tool: ${CAPTURE_TOOL}"

nrfutil device reset --serial-number "${HUB_SN}" 2>&1 | tail -1
sleep 8

python3 "${CAPTURE_TOOL}" \
    "${HUB_TTY}" \
    --settle-seconds 5 \
    --sample-seconds "${SAMPLE_SEC}" \
    --expected-nodes 9 \
    --csv "${PREFIX}.csv" \
    --summary "${PREFIX}.summary" \
    > "${PREFIX}.stdout.log" 2>&1 &
CAP_PID=$!
echo "capture pid=${CAP_PID}"

sleep $((5 + FAULT_AT_SEC))
echo "=== T+${FAULT_AT_SEC}: injecting Hub reset ==="
FAULT_WALL=$(date +%s)
nrfutil device reset --serial-number "${HUB_SN}" 2>&1 | tail -1
echo "fault wall=${FAULT_WALL}"

wait "${CAP_PID}"
RC=$?

echo "=== capture exited rc=${RC} ==="
echo
cat "${PREFIX}.summary" 2>&1 | tail -25
