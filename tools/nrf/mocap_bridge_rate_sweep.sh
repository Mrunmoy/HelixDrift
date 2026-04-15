#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REMOTE_HOST="${1:?usage: $0 <remote-host> <remote-repo> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [settle-seconds] [periods-ms] [artifact-dir]}"
REMOTE_REPO="${2:?usage: $0 <remote-host> <remote-repo> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [settle-seconds] [periods-ms] [artifact-dir]}"
REMOTE_JLINK_SERIAL="${3:-123456}"
DEVICE="${4:-NRF52840_XXAA}"
LOCAL_JLINK_SERIAL="${5:-69656876}"
DONGLE_TTY="${6:-/dev/ttyACM3}"
SAMPLE_SECONDS="${7:-10}"
SETTLE_SECONDS="${8:-30}"
PERIODS_MS="${9:-20 10}"
ARTIFACT_DIR="${10:-artifacts/rf/rate_sweep}"
REMOTE_NODE_ID="${REMOTE_NODE_ID:-2}"
LOCAL_NODE_ID="${LOCAL_NODE_ID:-1}"

mkdir -p "${ARTIFACT_DIR}"
SWEEP_CSV="${ARTIFACT_DIR}/mocap_bridge_rate_sweep.csv"

printf 'send_period_ms,rate_hz_node1,rate_hz_node2,rate_hz_combined,gap_per_1k_node1,gap_per_1k_node2,sync_delta_median_us,sync_delta_p90_us,sync_delta_p99_us,sync_delta_max_us,summary_file\n' > "${SWEEP_CSV}"

extract_field() {
  local pattern="$1"
  local file="$2"
  python3 - "$pattern" "$file" <<'PY'
import re
import sys
pattern = sys.argv[1]
path = sys.argv[2]
text = open(path, encoding='utf-8').read()
m = re.search(pattern, text, re.MULTILINE)
print(m.group(1) if m else "")
PY
}

for period_ms in ${PERIODS_MS}; do
  prefix="${ARTIFACT_DIR}/mocap_bridge_${period_ms}ms"
  "${REPO_ROOT}/tools/nrf/mocap_bridge_characterize.sh" \
    "${REMOTE_HOST}" \
    "${REMOTE_REPO}" \
    "${REMOTE_JLINK_SERIAL}" \
    "${DEVICE}" \
    "${LOCAL_JLINK_SERIAL}" \
    "${DONGLE_TTY}" \
    "${SAMPLE_SECONDS}" \
    "${REMOTE_NODE_ID}" \
    "${LOCAL_NODE_ID}" \
    "${period_ms}" \
    "${prefix}" \
    "${SETTLE_SECONDS}"

  summary_file="${prefix}.summary"
  rate1="$(extract_field 'RATE node=1 hz=([0-9.]+)' "${summary_file}")"
  rate2="$(extract_field 'RATE node=2 hz=([0-9.]+)' "${summary_file}")"
  rate_combined="$(extract_field 'RATE combined_hz=([0-9.]+)' "${summary_file}")"
  gap1="$(extract_field 'RATE node=1 hz=[0-9.]+ gap_per_1k=([0-9.]+)' "${summary_file}")"
  gap2="$(extract_field 'RATE node=2 hz=[0-9.]+ gap_per_1k=([0-9.]+)' "${summary_file}")"
  median="$(extract_field 'SYNC_DELTA_US .*median=([0-9]+)' "${summary_file}")"
  p90="$(extract_field 'SYNC_DELTA_US .*p90=([0-9]+)' "${summary_file}")"
  p99="$(extract_field 'SYNC_DELTA_US .*p99=([0-9]+)' "${summary_file}")"
  maxv="$(extract_field 'SYNC_DELTA_US .*max=([0-9]+)' "${summary_file}")"

  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "${period_ms}" \
    "${rate1}" \
    "${rate2}" \
    "${rate_combined}" \
    "${gap1}" \
    "${gap2}" \
    "${median}" \
    "${p90}" \
    "${p99}" \
    "${maxv}" \
    "${summary_file}" >> "${SWEEP_CSV}"
done

cat "${SWEEP_CSV}"
