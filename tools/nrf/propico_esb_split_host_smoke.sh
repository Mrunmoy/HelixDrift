#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v JLinkExe >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REMOTE_HOST="${1:-litu@hpserver1}"
REMOTE_REPO="${2:-/home/litu/sandbox/embedded/HelixDrift}"
LOCAL_JLINK_SERIAL="${3:-123456}"
REMOTE_JLINK_SERIAL="${4:-123456}"
DEVICE="${5:-NRF52840_XXAA}"
SETTLE_MS="${6:-3000}"

LOCAL_MASTER_BUILD=".deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-master-node1/nrf52840propico-esb-link/zephyr/zephyr.elf"
REMOTE_NODE_BUILD=".deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-node-node2/nrf52840propico-esb-link/zephyr/zephyr.elf"

parse_words() {
  awk '
    /^[0-9A-Fa-f]+ =/ {
      for (i = 3; i <= NF; ++i) {
        print $i;
      }
    }'
}

hex_to_dec() {
  printf '%d\n' "$((16#$1))"
}

run_local_status() {
  local addr
  addr="$(nix develop --command bash -lc "arm-none-eabi-nm -n '${REPO_ROOT}/${LOCAL_MASTER_BUILD}' | awk '\$3==\"g_helixEsbStatus\" {print \"0x\"\$1; exit}'")"
  JLinkExe -USB "${LOCAL_JLINK_SERIAL}" -Device "${DEVICE}" -If SWD -Speed 1000 -AutoConnect 1 -NoGui 1 <<EOF
connect
r
g
sleep ${SETTLE_MS}
halt
mem32 ${addr}, 16
go
q
EOF
}

run_remote_status() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ADDR=\$(arm-none-eabi-nm -n '${REMOTE_NODE_BUILD}' | awk '\$3==\"g_helixEsbStatus\" {print \"0x\"\$1; exit}') && JLinkExe -USB '${REMOTE_JLINK_SERIAL}' -Device '${DEVICE}' -If SWD -Speed 1000 -AutoConnect 1 -NoGui 1 <<EOF
connect
r
g
sleep ${SETTLE_MS}
halt
mem32 \${ADDR}, 16
go
q
EOF"
}

LOCAL_STATUS_FILE="$(mktemp)"
REMOTE_STATUS_FILE="$(mktemp)"
trap 'rm -f "${LOCAL_STATUS_FILE}" "${REMOTE_STATUS_FILE}"' EXIT

echo "== local master build + flash =="
nix develop --command bash -lc "'${REPO_ROOT}/tools/nrf/build_propico_esb_link.sh' master promicro_nrf52840/nrf52840/uf2 1 >/dev/null"
"${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" \
  "${REPO_ROOT}/.deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-master-node1/merged.hex" \
  "${LOCAL_JLINK_SERIAL}" \
  "${DEVICE}" \
  0 >/dev/null

echo "== sync remote workspace =="
"${REPO_ROOT}/tools/dev/sync_remote_workspace.sh" "${REMOTE_HOST}" "${REMOTE_REPO}" >/dev/null

echo "== remote node build + flash =="
ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && nix develop --command bash -lc './tools/nrf/build_propico_esb_link.sh node promicro_nrf52840/nrf52840/uf2 2 >/dev/null && ./tools/nrf/flash_hex_jlink.sh .deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-node-node2/merged.hex ${REMOTE_JLINK_SERIAL} ${DEVICE} 0 >/dev/null'"

echo "== sample local master status =="
run_local_status > "${LOCAL_STATUS_FILE}" || true
mapfile -t LOCAL_WORDS < <(parse_words < "${LOCAL_STATUS_FILE}")

echo "== sample remote node status =="
run_remote_status > "${REMOTE_STATUS_FILE}" || true
mapfile -t REMOTE_WORDS < <(parse_words < "${REMOTE_STATUS_FILE}")

if [[ ${#LOCAL_WORDS[@]} -lt 14 || ${#REMOTE_WORDS[@]} -lt 14 ]]; then
  echo "error: could not parse full status blocks" >&2
  exit 1
fi

LOCAL_MAGIC="${LOCAL_WORDS[0]}"
LOCAL_ROLE="$(hex_to_dec "${LOCAL_WORDS[1]}")"
LOCAL_PHASE="$(hex_to_dec "${LOCAL_WORDS[2]}")"
LOCAL_RX_PACKETS="$(hex_to_dec "${LOCAL_WORDS[7]}")"
LOCAL_LAST_RX_NODE="$(hex_to_dec "${LOCAL_WORDS[10]}")"
LOCAL_LAST_RX_LEN="$(hex_to_dec "${LOCAL_WORDS[11]}")"

REMOTE_MAGIC="${REMOTE_WORDS[0]}"
REMOTE_ROLE="$(hex_to_dec "${REMOTE_WORDS[1]}")"
REMOTE_PHASE="$(hex_to_dec "${REMOTE_WORDS[2]}")"
REMOTE_TX_SUCCESS="$(hex_to_dec "${REMOTE_WORDS[5]}")"
REMOTE_TX_FAILED="$(hex_to_dec "${REMOTE_WORDS[6]}")"
REMOTE_RX_PACKETS="$(hex_to_dec "${REMOTE_WORDS[7]}")"
REMOTE_LAST_RX_NODE="$(hex_to_dec "${REMOTE_WORDS[10]}")"
REMOTE_LAST_RX_LEN="$(hex_to_dec "${REMOTE_WORDS[11]}")"

printf 'local:  magic=%s role=%d phase=%d rx_packets=%d last_rx_node=%d last_rx_len=%d\n' \
  "${LOCAL_MAGIC}" "${LOCAL_ROLE}" "${LOCAL_PHASE}" "${LOCAL_RX_PACKETS}" "${LOCAL_LAST_RX_NODE}" "${LOCAL_LAST_RX_LEN}"
printf 'remote: magic=%s role=%d phase=%d tx_success=%d tx_failed=%d rx_packets=%d last_rx_node=%d last_rx_len=%d\n' \
  "${REMOTE_MAGIC}" "${REMOTE_ROLE}" "${REMOTE_PHASE}" "${REMOTE_TX_SUCCESS}" "${REMOTE_TX_FAILED}" "${REMOTE_RX_PACKETS}" "${REMOTE_LAST_RX_NODE}" "${REMOTE_LAST_RX_LEN}"

if [[ "${LOCAL_MAGIC}" != "48455342" || "${REMOTE_MAGIC}" != "48455342" ]]; then
  echo "error: bad status magic" >&2
  exit 1
fi

if (( LOCAL_ROLE != 1 || LOCAL_PHASE != 4 || LOCAL_RX_PACKETS == 0 || LOCAL_LAST_RX_NODE != 2 || LOCAL_LAST_RX_LEN != 4 )); then
  echo "error: local master did not observe node traffic" >&2
  exit 1
fi

if (( REMOTE_ROLE != 2 || REMOTE_PHASE != 4 || REMOTE_TX_SUCCESS == 0 || REMOTE_TX_FAILED != 0 || REMOTE_RX_PACKETS == 0 || REMOTE_LAST_RX_NODE != 1 || REMOTE_LAST_RX_LEN != 4 )); then
  echo "error: remote node did not complete successful ESB exchange" >&2
  exit 1
fi

echo "split-host ProPico ESB smoke: PASS"
