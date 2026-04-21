#!/usr/bin/env bash
# Phase D — 10-min stress test with repeated Hub resets.
#
# Hub resets at T=60, 180, 300, 420 s (every 2 min after the first).
# Measures whether each recovery succeeds and whether recovery time
# stays stable across repeated resets.
set -u
SAMPLE_SEC="${1:-600}"
ARTDIR="${HELIX_RF_ARTIFACT_DIR:-/tmp/helix_tag_log}"
PREFIX="${2:-${ARTDIR}/rf_phased_multireset_$(date +%Y%m%d_%H%M%S)}"
HUB_SN="${HELIX_HUB_JLINK_SN:-69656876}"
HUB_TTY="${HELIX_HUB_TTY:-/dev/ttyACM1}"

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CAPTURE_TOOL="${REPO_ROOT}/tools/analysis/capture_mocap_bridge_robust.py"

mkdir -p "$(dirname "$PREFIX")"
cd "$REPO_ROOT"

echo "=== Phase D — multi-reset stress (${SAMPLE_SEC}s capture) ==="
echo "Hub resets at T = 60, 180, 300, 420 s"
echo "Artifact        : ${PREFIX}"
echo "Capture tool    : ${CAPTURE_TOOL}"

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

START_WALL=$(date +%s)
echo "capture pid=${CAP_PID}, start wall=${START_WALL}"

# Wait for settle + first fault
sleep $((5 + 60))
for T in 60 180 300 420; do
    if [ "$T" != "60" ]; then
        # For second+ fault, sleep the delta from previous
        PREV_T=$PREV
        sleep $((T - PREV_T))
    fi
    FAULT_WALL=$(date +%s)
    REL_T=$((FAULT_WALL - START_WALL - 5))
    echo "=== T+${REL_T}s (target T=${T}s): Hub reset #${T} at wall ${FAULT_WALL} ==="
    nrfutil device reset --serial-number "${HUB_SN}" > /dev/null 2>&1
    PREV=$T
done

wait "${CAP_PID}"
echo
echo "=== Summary ==="
tail -30 "${PREFIX}.summary"
