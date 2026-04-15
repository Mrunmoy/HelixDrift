#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REMOTE_HOST="${1:?usage: $0 <remote-host> <remote-repo> <uf2-mount-3> <uf2-mount-4> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [send-period-ms]}"
REMOTE_REPO="${2:?usage: $0 <remote-host> <remote-repo> <uf2-mount-3> <uf2-mount-4> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [send-period-ms]}"
UF2_MOUNT_3="${3:?usage: $0 <remote-host> <remote-repo> <uf2-mount-3> <uf2-mount-4> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [send-period-ms]}"
UF2_MOUNT_4="${4:?usage: $0 <remote-host> <remote-repo> <uf2-mount-3> <uf2-mount-4> [remote-jlink-serial] [device] [local-jlink-serial] [dongle-tty] [sample-seconds] [send-period-ms]}"
REMOTE_JLINK_SERIAL="${5:-123456}"
DEVICE="${6:-NRF52840_XXAA}"
LOCAL_JLINK_SERIAL="${7:-69656876}"
DONGLE_TTY="${8:-/dev/ttyACM3}"
SAMPLE_SECONDS="${9:-10}"
SEND_PERIOD_MS="${10:-20}"

NCS_VERSION="${NCS_VERSION:-v3.2.4}"
WORKSPACE_DIR="${NCS_WORKSPACE_DIR:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"
CENTRAL_HEX="${WORKSPACE_DIR}/build-helix-nrf52840dongle-nrf52840-bare-mocap-central-node1/nrf52840-mocap-bridge/zephyr/zephyr.hex"
NODE1_HEX="${WORKSPACE_DIR}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node1/nrf52840-mocap-bridge/zephyr/zephyr.hex"
NODE3_UF2="${WORKSPACE_DIR}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node3/nrf52840-mocap-bridge/zephyr/zephyr.uf2"
NODE4_UF2="${WORKSPACE_DIR}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node4/nrf52840-mocap-bridge/zephyr/zephyr.uf2"

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
  "${REPO_ROOT}/tools/nrf/build_mocap_bridge.sh" node promicro_nrf52840/nrf52840/uf2 1 \
    "-DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=${SEND_PERIOD_MS}"
  "${REPO_ROOT}/tools/nrf/build_mocap_bridge.sh" node promicro_nrf52840/nrf52840/uf2 3 \
    "-DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=${SEND_PERIOD_MS}"
  "${REPO_ROOT}/tools/nrf/build_mocap_bridge.sh" node promicro_nrf52840/nrf52840/uf2 4 \
    "-DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=${SEND_PERIOD_MS}"
}

build_remote() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ./tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840/uf2 2 '-DCONFIG_HELIX_MOCAP_SEND_PERIOD_MS=${SEND_PERIOD_MS}'"
}

flash_swd_nodes() {
  "${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" "${CENTRAL_HEX}" "${LOCAL_JLINK_SERIAL}" "${DEVICE}" 0
  "${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" "${NODE1_HEX}" 123456 "${DEVICE}" 0
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ./tools/nrf/flash_hex_jlink.sh '.deps/ncs/${NCS_VERSION}/build-helix-promicro_nrf52840-nrf52840-uf2-mocap-node-node2/nrf52840-mocap-bridge/zephyr/zephyr.hex' '${REMOTE_JLINK_SERIAL}' '${DEVICE}' 0"
}

flash_uf2_nodes() {
  "${REPO_ROOT}/tools/nrf/flash_uf2_volume.sh" "${NODE3_UF2}" "${UF2_MOUNT_3}"
  "${REPO_ROOT}/tools/nrf/flash_uf2_volume.sh" "${NODE4_UF2}" "${UF2_MOUNT_4}"
}

capture() {
  local resolved_tty
  resolved_tty="$(resolve_dongle_tty "${DONGLE_TTY}")"
  echo "Using dongle tty: ${resolved_tty}"
  "${REPO_ROOT}/tools/analysis/capture_mocap_bridge_window.py" \
    "${resolved_tty}" \
    --settle-seconds 30 \
    --sample-seconds "${SAMPLE_SECONDS}" \
    --expected-nodes 4 \
    --csv "artifacts/rf/mocap_bridge_four_node_${SEND_PERIOD_MS}ms.csv" \
    --summary "artifacts/rf/mocap_bridge_four_node_${SEND_PERIOD_MS}ms.summary"
}

build_local
build_remote
flash_swd_nodes
flash_uf2_nodes
capture
