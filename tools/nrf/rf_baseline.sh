#!/usr/bin/env bash
# RF robustness Phase A — steady-state baseline capture for the 10-Tag fleet.
#
# Resets Hub cleanly, waits for all 10 Tags to start streaming, then
# captures N seconds of Hub USB CDC. Emits per-Tag rate / gap_per_1k /
# combined_hz / sync_span via capture_mocap_bridge_window.py.
#
# Assumes:
#   - Hub on /dev/ttyACM1 (USB CDC)
#   - Hub J-Link SN 69656876
#   - All 10 Tags on v-whatever with node_ids 1..10 flash-provisioned
#   - All 10 Tags powered (native USB) and past their 300 s OTA window
#     (if not, just wait — they'll drop into ESB)
set -u
SAMPLE_SEC="${1:-600}"   # default 10 min capture
SETTLE_SEC="${2:-10}"    # default 10 s settle
ARTDIR="${HELIX_RF_ARTIFACT_DIR:-/tmp/helix_tag_log}"
PREFIX="${3:-${ARTDIR}/rf_baseline_$(date +%Y%m%d_%H%M%S)}"
HUB_SN="${HELIX_HUB_JLINK_SN:-69656876}"
HUB_TTY="${HELIX_HUB_TTY:-/dev/ttyACM1}"

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CAPTURE_TOOL="${REPO_ROOT}/tools/analysis/capture_mocap_bridge_window.py"

mkdir -p "$(dirname "$PREFIX")"
cd "$REPO_ROOT"

echo "=== RF baseline capture ==="
echo "Sample seconds : ${SAMPLE_SEC}"
echo "Settle seconds : ${SETTLE_SEC}"
echo "Artifact prefix: ${PREFIX}"
echo

echo "Resetting Hub..."
nrfutil device reset --serial-number "${HUB_SN}" 2>&1 | tail -1
sleep 8

echo "Capturing (will take $((SAMPLE_SEC + SETTLE_SEC))s)..."
python3 "${CAPTURE_TOOL}" \
    "${HUB_TTY}" \
    --settle-seconds "${SETTLE_SEC}" \
    --sample-seconds "${SAMPLE_SEC}" \
    --expected-nodes 10 \
    --csv   "${PREFIX}.csv" \
    --summary "${PREFIX}.summary"

echo
echo "=== Done ==="
echo "CSV    : ${PREFIX}.csv"
echo "Summary: ${PREFIX}.summary"
