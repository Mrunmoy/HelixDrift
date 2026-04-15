#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REMOTE_HOST="${1:?usage: $0 <remote-host> <remote-repo> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [remote-node-id] [local-node-id] [send-period-ms] [artifact-prefix] [settle-seconds]}"
REMOTE_REPO="${2:?usage: $0 <remote-host> <remote-repo> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [remote-node-id] [local-node-id] [send-period-ms] [artifact-prefix] [settle-seconds]}"
REMOTE_JLINK_SERIAL="${3:-123456}"
DEVICE="${4:-NRF52840_XXAA}"
LOCAL_JLINK_SERIAL="${5:-69656876}"
DONGLE_TTY="${6:-/dev/ttyACM3}"
SAMPLE_SECONDS="${7:-20}"
REMOTE_NODE_ID="${8:-2}"
LOCAL_NODE_ID="${9:-1}"
SEND_PERIOD_MS="${10:-20}"
ARTIFACT_PREFIX="${11:-artifacts/rf/mocap_bridge_two_node_${SEND_PERIOD_MS}ms}"
SETTLE_SECONDS="${12:-30}"

NCS_VERSION="${NCS_VERSION:-v3.2.4}"
WORKSPACE_DIR="${NCS_WORKSPACE_DIR:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"
LOCAL_NODE_HEX="${WORKSPACE_DIR}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node${LOCAL_NODE_ID}/nrf52840-mocap-bridge/zephyr/zephyr.hex"
CENTRAL_HEX="${WORKSPACE_DIR}/build-helix-nrf52840dongle-nrf52840-bare-mocap-central-node1/nrf52840-mocap-bridge/zephyr/zephyr.hex"

resolve_dongle_tty() {
  local requested="$1"
  if [[ -e "${requested}" ]]; then
    printf '%s\n' "${requested}"
    return 0
  fi

  python3 - <<'PY'
from serial.tools import list_ports

matches = []
for port in list_ports.comports():
    product = port.product or ""
    manufacturer = port.manufacturer or ""
    if product == "Helix Mocap Central" or manufacturer == "HelixDrift":
        matches.append(port.device)

if not matches:
    raise SystemExit("failed to resolve Helix Mocap Central serial port")

print(matches[0])
PY
}

build_local() {
  "${REPO_ROOT}/tools/nrf/build_mocap_bridge.sh" central nrf52840dongle/nrf52840/bare 1
  "${REPO_ROOT}/tools/nrf/build_mocap_bridge.sh" node promicro_nrf52840/nrf52840/uf2 "${LOCAL_NODE_ID}" \
    "-DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=${SEND_PERIOD_MS}"
}

flash_local() {
  "${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" "${CENTRAL_HEX}" "${LOCAL_JLINK_SERIAL}" "${DEVICE}" 0
  "${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" "${LOCAL_NODE_HEX}" 123456 "${DEVICE}" 0
}

build_remote() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ./tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 '${REMOTE_NODE_ID}' '-DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=${SEND_PERIOD_MS}'"
}

flash_remote() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ./tools/nrf/flash_hex_jlink.sh '.deps/ncs/${NCS_VERSION}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node${REMOTE_NODE_ID}/nrf52840-mocap-bridge/zephyr/zephyr.hex' '${REMOTE_JLINK_SERIAL}' '${DEVICE}' 0"
}

capture() {
  local resolved_tty
  resolved_tty="$(resolve_dongle_tty "${DONGLE_TTY}")"
  echo "Using dongle tty: ${resolved_tty}"
  "${REPO_ROOT}/tools/analysis/capture_mocap_bridge_window.py" \
    "${resolved_tty}" \
    --settle-seconds "${SETTLE_SECONDS}" \
    --sample-seconds "${SAMPLE_SECONDS}" \
    --csv "${ARTIFACT_PREFIX}.csv" \
    --summary "${ARTIFACT_PREFIX}.summary"
}

build_local
build_remote
flash_local
flash_remote
capture
