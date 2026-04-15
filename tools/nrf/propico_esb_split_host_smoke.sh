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
MODE="${7:-baseline}"
MASTER_BLACKOUT_START_SEQUENCE="${8:-10}"
MASTER_BLACKOUT_LENGTH="${9:-5}"
SESSION_TAG="${10:-66}"
NODE_TX_BLACKOUT_START_SEQUENCE="${11:-10}"
NODE_TX_BLACKOUT_LENGTH="${12:-5}"
SOAK_SAMPLES="${HELIX_ESB_SOAK_SAMPLES:-5}"
SOAK_INTERVAL_MS="${HELIX_ESB_SOAK_INTERVAL_MS:-2000}"
SOAK_CSV="${HELIX_ESB_SOAK_CSV:-}"
SOAK_SUMMARY="${HELIX_ESB_SOAK_SUMMARY:-}"
STATUS_CAPTURE_DELAY_MS="${SETTLE_MS}"

LOCAL_MASTER_BUILD=".deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-master-node1/nrf52840propico-esb-link/zephyr/zephyr.elf"
REMOTE_NODE_BUILD=".deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-node-node2/nrf52840propico-esb-link/zephyr/zephyr.elf"
EXPECTED_BLACKOUT_GAP_MIN_US=200000
REMOTE_TX_FAILED_LIMIT=2

if [[ "${MODE}" != "baseline" && "${MODE}" != "blackout" && "${MODE}" != "dropout" && "${MODE}" != "characterize" ]]; then
  echo "usage: $0 [remote_host] [remote_repo] [local_jlink] [remote_jlink] [device] [settle_ms] [baseline|blackout|dropout|characterize] [blackout_start_seq] [blackout_len] [session_tag] [node_tx_blackout_start_seq] [node_tx_blackout_len]" >&2
  exit 2
fi

parse_words() {
  awk '
    /^[0-9A-Fa-f]+ =/ {
      for (i = 3; i <= NF; ++i) {
        print $i;
      }
    }'
}

detect_local_jlink_serial() {
  JLinkExe -NoGui 1 <<'EOF' 2>/dev/null | awk '/^S\/N:/ { print $2; exit }'
USB
q
EOF
}

if [[ "${LOCAL_JLINK_SERIAL}" == "123456" ]]; then
  DETECTED_LOCAL_JLINK_SERIAL="$(detect_local_jlink_serial || true)"
  if [[ -n "${DETECTED_LOCAL_JLINK_SERIAL}" ]]; then
    LOCAL_JLINK_SERIAL="${DETECTED_LOCAL_JLINK_SERIAL}"
    echo "auto-detected local J-Link serial: ${LOCAL_JLINK_SERIAL}"
  fi
fi

capture_status_words() {
  local output_file="$1"
  local words_name="$2"
  local reader="$3"
  local attempts="${4:-3}"
  local -n words_ref="${words_name}"
  local attempt

  words_ref=()
  for (( attempt = 1; attempt <= attempts; ++attempt )); do
    "${reader}" > "${output_file}" || true
    mapfile -t words_ref < <(parse_words < "${output_file}")
    if [[ ${#words_ref[@]} -ge 39 ]]; then
      return 0
    fi
    sleep 1
  done

  return 1
}

hex_to_dec() {
  printf '%d\n' "$((16#$1))"
}

hex_to_s32() {
  local value
  value=$((16#$1))
  if (( value >= 0x80000000 )); then
    value=$((value - 0x100000000))
  fi
  printf '%d\n' "${value}"
}

resolve_local_status_addr() {
  nix develop --command bash -lc "arm-none-eabi-nm -n '${REPO_ROOT}/${LOCAL_MASTER_BUILD}' | awk '\$3==\"g_helixEsbStatus\" {print \"0x\"\$1; exit}'"
}

resolve_remote_status_addr() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && arm-none-eabi-nm -n '${REMOTE_NODE_BUILD}' | awk '\$3==\"g_helixEsbStatus\" {print \"0x\"\$1; exit}'"
}

run_local_status_reset() {
  JLinkExe -USB "${LOCAL_JLINK_SERIAL}" -Device "${DEVICE}" -If SWD -Speed 1000 -AutoConnect 1 -NoGui 1 <<EOF
connect
r
g
sleep ${STATUS_CAPTURE_DELAY_MS}
halt
mem32 ${LOCAL_STATUS_ADDR}, 39
go
q
EOF
}

run_remote_status_reset() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && JLinkExe -USB '${REMOTE_JLINK_SERIAL}' -Device '${DEVICE}' -If SWD -Speed 1000 -AutoConnect 1 -NoGui 1 <<EOF
connect
r
g
sleep ${STATUS_CAPTURE_DELAY_MS}
halt
mem32 ${REMOTE_STATUS_ADDR}, 39
go
q
EOF"
}

run_local_status_live() {
  JLinkExe -USB "${LOCAL_JLINK_SERIAL}" -Device "${DEVICE}" -If SWD -Speed 1000 -AutoConnect 1 -NoGui 1 <<EOF
connect
sleep ${STATUS_CAPTURE_DELAY_MS}
halt
mem32 ${LOCAL_STATUS_ADDR}, 39
go
q
EOF
}

run_remote_status_live() {
  ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && JLinkExe -USB '${REMOTE_JLINK_SERIAL}' -Device '${DEVICE}' -If SWD -Speed 1000 -AutoConnect 1 -NoGui 1 <<EOF
connect
sleep ${STATUS_CAPTURE_DELAY_MS}
halt
mem32 ${REMOTE_STATUS_ADDR}, 39
go
q
EOF"
}

LOCAL_STATUS_FILE="$(mktemp)"
REMOTE_STATUS_FILE="$(mktemp)"
trap 'rm -f "${LOCAL_STATUS_FILE}" "${REMOTE_STATUS_FILE}"' EXIT

MASTER_BUILD_ARGS=(master promicro_nrf52840/nrf52840/uf2 1)
MASTER_BUILD_ARGS+=(
  "-DCONFIG_HELIX_ESB_SESSION_TAG=${SESSION_TAG}"
  "-DCONFIG_HELIX_ESB_MASTER_BLACKOUT_START_SEQUENCE=0"
  "-DCONFIG_HELIX_ESB_MASTER_BLACKOUT_LENGTH=0"
)
if [[ "${MODE}" == "blackout" ]]; then
  MASTER_BUILD_ARGS+=(
    "-DCONFIG_HELIX_ESB_MASTER_BLACKOUT_START_SEQUENCE=${MASTER_BLACKOUT_START_SEQUENCE}"
    "-DCONFIG_HELIX_ESB_MASTER_BLACKOUT_LENGTH=${MASTER_BLACKOUT_LENGTH}"
  )
fi

NODE_BUILD_ARGS=(node promicro_nrf52840/nrf52840/uf2 2)
NODE_BUILD_ARGS+=(
  "-DCONFIG_HELIX_ESB_SESSION_TAG=${SESSION_TAG}"
  "-DCONFIG_HELIX_ESB_NODE_TX_BLACKOUT_START_SEQUENCE=0"
  "-DCONFIG_HELIX_ESB_NODE_TX_BLACKOUT_LENGTH=0"
)
if [[ "${MODE}" == "dropout" ]]; then
  NODE_BUILD_ARGS+=(
    "-DCONFIG_HELIX_ESB_NODE_TX_BLACKOUT_START_SEQUENCE=${NODE_TX_BLACKOUT_START_SEQUENCE}"
    "-DCONFIG_HELIX_ESB_NODE_TX_BLACKOUT_LENGTH=${NODE_TX_BLACKOUT_LENGTH}"
  )
  REMOTE_TX_FAILED_LIMIT=8
fi
if [[ "${MODE}" == "characterize" && -z "${SOAK_CSV}" ]]; then
  SOAK_CSV="${REPO_ROOT}/artifacts/rf/propico_esb_characterize_session${SESSION_TAG}.csv"
fi
if [[ "${MODE}" == "characterize" && -z "${SOAK_SUMMARY}" ]]; then
  SOAK_SUMMARY="${SOAK_CSV%.csv}.summary"
fi

echo "== local master build + flash =="
"${REPO_ROOT}/tools/nrf/build_propico_esb_link.sh" "${MASTER_BUILD_ARGS[@]}" >/dev/null
"${REPO_ROOT}/tools/nrf/flash_hex_jlink.sh" \
  "${REPO_ROOT}/.deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-master-node1/merged.hex" \
  "${LOCAL_JLINK_SERIAL}" \
  "${DEVICE}" \
  0 >/dev/null

echo "== sync remote workspace =="
"${REPO_ROOT}/tools/dev/sync_remote_workspace.sh" "${REMOTE_HOST}" "${REMOTE_REPO}" >/dev/null

echo "== remote node build + flash =="
ssh "${REMOTE_HOST}" "cd '${REMOTE_REPO}' && ./tools/nrf/build_propico_esb_link.sh ${NODE_BUILD_ARGS[*]} >/dev/null && ./tools/nrf/flash_hex_jlink.sh .deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-uf2-esb-node-node2/merged.hex ${REMOTE_JLINK_SERIAL} ${DEVICE} 0 >/dev/null"

LOCAL_STATUS_ADDR="$(resolve_local_status_addr)"
REMOTE_STATUS_ADDR="$(resolve_remote_status_addr)"

echo "== sample local master status =="
capture_status_words "${LOCAL_STATUS_FILE}" LOCAL_WORDS run_local_status_reset || true

echo "== sample remote node status =="
capture_status_words "${REMOTE_STATUS_FILE}" REMOTE_WORDS run_remote_status_reset || true

if [[ ${#LOCAL_WORDS[@]} -lt 39 || ${#REMOTE_WORDS[@]} -lt 39 ]]; then
  echo "local parsed words: ${#LOCAL_WORDS[@]}" >&2
  echo "remote parsed words: ${#REMOTE_WORDS[@]}" >&2
  echo "local status tail:" >&2
  tail -n 20 "${LOCAL_STATUS_FILE}" >&2 || true
  echo "remote status tail:" >&2
  tail -n 20 "${REMOTE_STATUS_FILE}" >&2 || true
  echo "error: could not parse full status blocks" >&2
  exit 1
fi

decode_status_words() {
  LOCAL_MAGIC="${LOCAL_WORDS[0]}"
  LOCAL_ROLE="$(hex_to_dec "${LOCAL_WORDS[1]}")"
  LOCAL_PHASE="$(hex_to_dec "${LOCAL_WORDS[2]}")"
  LOCAL_RX_PACKETS="$(hex_to_dec "${LOCAL_WORDS[7]}")"
  LOCAL_ACK_PAYLOADS="$(hex_to_dec "${LOCAL_WORDS[8]}")"
  LOCAL_LAST_RX_NODE="$(hex_to_dec "${LOCAL_WORDS[10]}")"
  LOCAL_LAST_RX_LEN="$(hex_to_dec "${LOCAL_WORDS[11]}")"
  LOCAL_ANCHORS_SENT="$(hex_to_dec "${LOCAL_WORDS[15]}")"
  LOCAL_LAST_ANCHOR_SEQUENCE="$(hex_to_dec "${LOCAL_WORDS[16]}")"
  LOCAL_LAST_MASTER_TIMESTAMP_US="$(hex_to_dec "${LOCAL_WORDS[18]}")"
  LOCAL_ANCHOR_RAW_WORD0="$(hex_to_dec "${LOCAL_WORDS[20]}")"
  LOCAL_ANCHOR_RAW_WORD1="$(hex_to_dec "${LOCAL_WORDS[21]}")"
  LOCAL_ANCHORS_SUPPRESSED="$(hex_to_dec "${LOCAL_WORDS[30]}")"
  LOCAL_FRAME_SEQUENCE_GAPS="$(hex_to_dec "${LOCAL_WORDS[36]}")"
  LOCAL_FRAME_RECOVERY_EVENTS="$(hex_to_dec "${LOCAL_WORDS[37]}")"
  LOCAL_FRAME_MISSING_COUNT="$(hex_to_dec "${LOCAL_WORDS[38]}")"

  REMOTE_MAGIC="${REMOTE_WORDS[0]}"
  REMOTE_ROLE="$(hex_to_dec "${REMOTE_WORDS[1]}")"
  REMOTE_PHASE="$(hex_to_dec "${REMOTE_WORDS[2]}")"
  REMOTE_TX_SUCCESS="$(hex_to_dec "${REMOTE_WORDS[5]}")"
  REMOTE_TX_FAILED="$(hex_to_dec "${REMOTE_WORDS[6]}")"
  REMOTE_RX_PACKETS="$(hex_to_dec "${REMOTE_WORDS[7]}")"
  REMOTE_ACK_PAYLOADS="$(hex_to_dec "${REMOTE_WORDS[8]}")"
  REMOTE_LAST_RX_NODE="$(hex_to_dec "${REMOTE_WORDS[10]}")"
  REMOTE_LAST_RX_LEN="$(hex_to_dec "${REMOTE_WORDS[11]}")"
  REMOTE_ANCHORS_RECEIVED="$(hex_to_dec "${REMOTE_WORDS[14]}")"
  REMOTE_LAST_ANCHOR_SEQUENCE="$(hex_to_dec "${REMOTE_WORDS[16]}")"
  REMOTE_ESTIMATED_OFFSET_US="$(hex_to_s32 "${REMOTE_WORDS[17]}")"
  REMOTE_LAST_MASTER_TIMESTAMP_US="$(hex_to_dec "${REMOTE_WORDS[18]}")"
  REMOTE_LAST_LOCAL_TIMESTAMP_US="$(hex_to_dec "${REMOTE_WORDS[19]}")"
  REMOTE_ANCHOR_RAW_WORD0="$(hex_to_dec "${REMOTE_WORDS[20]}")"
  REMOTE_ANCHOR_RAW_WORD1="$(hex_to_dec "${REMOTE_WORDS[21]}")"
  REMOTE_ANCHOR_SEQUENCE_GAPS="$(hex_to_dec "${REMOTE_WORDS[22]}")"
  REMOTE_OFFSET_MIN_US="$(hex_to_s32 "${REMOTE_WORDS[23]}")"
  REMOTE_OFFSET_MAX_US="$(hex_to_s32 "${REMOTE_WORDS[24]}")"
  REMOTE_LAST_ANCHOR_MASTER_DELTA_US="$(hex_to_s32 "${REMOTE_WORDS[25]}")"
  REMOTE_LAST_ANCHOR_LOCAL_DELTA_US="$(hex_to_s32 "${REMOTE_WORDS[26]}")"
  REMOTE_LAST_ANCHOR_SKEW_US="$(hex_to_s32 "${REMOTE_WORDS[27]}")"
  REMOTE_ANCHOR_SKEW_MIN_US="$(hex_to_s32 "${REMOTE_WORDS[28]}")"
  REMOTE_ANCHOR_SKEW_MAX_US="$(hex_to_s32 "${REMOTE_WORDS[29]}")"
  REMOTE_ANCHOR_RECOVERY_EVENTS="$(hex_to_dec "${REMOTE_WORDS[31]}")"
  REMOTE_ANCHOR_MISSING_COUNT="$(hex_to_dec "${REMOTE_WORDS[32]}")"
  REMOTE_MAX_ANCHOR_MASTER_DELTA_US="$(hex_to_s32 "${REMOTE_WORDS[33]}")"
  REMOTE_MAX_ANCHOR_LOCAL_DELTA_US="$(hex_to_s32 "${REMOTE_WORDS[34]}")"
  REMOTE_FRAMES_SUPPRESSED="$(hex_to_dec "${REMOTE_WORDS[35]}")"
}

decode_status_words

printf 'local:  magic=%s role=%d phase=%d rx_packets=%d ack_payloads=%d anchors_sent=%d anchors_suppressed=%d frame_gaps=%d frame_recoveries=%d frame_missing=%d last_rx_node=%d last_rx_len=%d anchor_seq=%d master_ts_us=%d raw0=0x%08X raw1=0x%08X\n' \
  "${LOCAL_MAGIC}" "${LOCAL_ROLE}" "${LOCAL_PHASE}" "${LOCAL_RX_PACKETS}" "${LOCAL_ACK_PAYLOADS}" "${LOCAL_ANCHORS_SENT}" "${LOCAL_ANCHORS_SUPPRESSED}" "${LOCAL_FRAME_SEQUENCE_GAPS}" "${LOCAL_FRAME_RECOVERY_EVENTS}" "${LOCAL_FRAME_MISSING_COUNT}" "${LOCAL_LAST_RX_NODE}" "${LOCAL_LAST_RX_LEN}" "${LOCAL_LAST_ANCHOR_SEQUENCE}" "${LOCAL_LAST_MASTER_TIMESTAMP_US}" "${LOCAL_ANCHOR_RAW_WORD0}" "${LOCAL_ANCHOR_RAW_WORD1}"
printf 'remote: magic=%s role=%d phase=%d tx_success=%d tx_failed=%d rx_packets=%d ack_payloads=%d anchors_received=%d last_rx_node=%d last_rx_len=%d anchor_seq=%d master_ts_us=%d local_ts_us=%d offset_us=%d raw0=0x%08X raw1=0x%08X seq_gaps=%d recoveries=%d missing=%d offset_min=%d offset_max=%d master_delta=%d local_delta=%d max_master_delta=%d max_local_delta=%d skew=%d skew_min=%d skew_max=%d\n' \
  "${REMOTE_MAGIC}" "${REMOTE_ROLE}" "${REMOTE_PHASE}" "${REMOTE_TX_SUCCESS}" "${REMOTE_TX_FAILED}" "${REMOTE_RX_PACKETS}" "${REMOTE_ACK_PAYLOADS}" "${REMOTE_ANCHORS_RECEIVED}" "${REMOTE_LAST_RX_NODE}" "${REMOTE_LAST_RX_LEN}" "${REMOTE_LAST_ANCHOR_SEQUENCE}" "${REMOTE_LAST_MASTER_TIMESTAMP_US}" "${REMOTE_LAST_LOCAL_TIMESTAMP_US}" "${REMOTE_ESTIMATED_OFFSET_US}" "${REMOTE_ANCHOR_RAW_WORD0}" "${REMOTE_ANCHOR_RAW_WORD1}" "${REMOTE_ANCHOR_SEQUENCE_GAPS}" "${REMOTE_ANCHOR_RECOVERY_EVENTS}" "${REMOTE_ANCHOR_MISSING_COUNT}" "${REMOTE_OFFSET_MIN_US}" "${REMOTE_OFFSET_MAX_US}" "${REMOTE_LAST_ANCHOR_MASTER_DELTA_US}" "${REMOTE_LAST_ANCHOR_LOCAL_DELTA_US}" "${REMOTE_MAX_ANCHOR_MASTER_DELTA_US}" "${REMOTE_MAX_ANCHOR_LOCAL_DELTA_US}" "${REMOTE_LAST_ANCHOR_SKEW_US}" "${REMOTE_ANCHOR_SKEW_MIN_US}" "${REMOTE_ANCHOR_SKEW_MAX_US}"
printf 'remote: frames_suppressed=%d\n' "${REMOTE_FRAMES_SUPPRESSED}"

if [[ "${LOCAL_MAGIC}" != "48455342" || "${REMOTE_MAGIC}" != "48455342" ]]; then
  echo "error: bad status magic" >&2
  exit 1
fi

if (( LOCAL_ROLE != 1 || LOCAL_PHASE != 4 || LOCAL_RX_PACKETS == 0 || LOCAL_ACK_PAYLOADS == 0 || LOCAL_ANCHORS_SENT == 0 || LOCAL_LAST_RX_NODE != 2 || LOCAL_LAST_RX_LEN != 12 || LOCAL_LAST_ANCHOR_SEQUENCE == 0 || LOCAL_LAST_MASTER_TIMESTAMP_US == 0 || (LOCAL_ANCHOR_RAW_WORD0 & 0xFF) != 0xA1 || LOCAL_ANCHOR_RAW_WORD1 == 0 )); then
  echo "error: local master did not observe node traffic" >&2
  exit 1
fi

if (( ((LOCAL_ANCHOR_RAW_WORD0 >> 24) & 0xFF) != SESSION_TAG )); then
  echo "error: local master status did not retain expected session tag" >&2
  exit 1
fi

if [[ "${MODE}" == "dropout" ]]; then
  if (( REMOTE_ROLE != 2 || REMOTE_PHASE != 4 || REMOTE_TX_SUCCESS == 0 || REMOTE_RX_PACKETS == 0 || REMOTE_ACK_PAYLOADS == 0 || REMOTE_ANCHORS_RECEIVED == 0 || REMOTE_LAST_RX_NODE != 1 || REMOTE_LAST_RX_LEN != 8 || REMOTE_LAST_ANCHOR_SEQUENCE == 0 || REMOTE_LAST_MASTER_TIMESTAMP_US == 0 || REMOTE_LAST_LOCAL_TIMESTAMP_US == 0 || REMOTE_ESTIMATED_OFFSET_US == 0 || (REMOTE_ANCHOR_RAW_WORD0 & 0xFF) != 0xA1 || ((REMOTE_ANCHOR_RAW_WORD0 >> 8) & 0xFF) != 0x01 || REMOTE_ANCHOR_RAW_WORD1 == 0 )); then
    echo "error: remote node did not complete successful ESB exchange" >&2
    exit 1
  fi
else
  if (( REMOTE_ROLE != 2 || REMOTE_PHASE != 4 || REMOTE_TX_SUCCESS == 0 || REMOTE_TX_FAILED > REMOTE_TX_FAILED_LIMIT || REMOTE_RX_PACKETS == 0 || REMOTE_ACK_PAYLOADS == 0 || REMOTE_ANCHORS_RECEIVED < 2 || REMOTE_LAST_RX_NODE != 1 || REMOTE_LAST_RX_LEN != 8 || REMOTE_LAST_ANCHOR_SEQUENCE == 0 || REMOTE_LAST_MASTER_TIMESTAMP_US == 0 || REMOTE_LAST_LOCAL_TIMESTAMP_US == 0 || REMOTE_ESTIMATED_OFFSET_US == 0 || (REMOTE_ANCHOR_RAW_WORD0 & 0xFF) != 0xA1 || ((REMOTE_ANCHOR_RAW_WORD0 >> 8) & 0xFF) != 0x01 || REMOTE_ANCHOR_RAW_WORD1 == 0 || REMOTE_OFFSET_MIN_US > REMOTE_OFFSET_MAX_US || REMOTE_ESTIMATED_OFFSET_US < REMOTE_OFFSET_MIN_US || REMOTE_ESTIMATED_OFFSET_US > REMOTE_OFFSET_MAX_US || REMOTE_LAST_ANCHOR_MASTER_DELTA_US <= 0 || REMOTE_LAST_ANCHOR_LOCAL_DELTA_US <= 0 || REMOTE_MAX_ANCHOR_MASTER_DELTA_US < REMOTE_LAST_ANCHOR_MASTER_DELTA_US || REMOTE_MAX_ANCHOR_LOCAL_DELTA_US < REMOTE_LAST_ANCHOR_LOCAL_DELTA_US || REMOTE_LAST_ANCHOR_SKEW_US != (REMOTE_LAST_ANCHOR_LOCAL_DELTA_US - REMOTE_LAST_ANCHOR_MASTER_DELTA_US) || REMOTE_ANCHOR_SKEW_MIN_US > REMOTE_ANCHOR_SKEW_MAX_US || REMOTE_LAST_ANCHOR_SKEW_US < REMOTE_ANCHOR_SKEW_MIN_US || REMOTE_LAST_ANCHOR_SKEW_US > REMOTE_ANCHOR_SKEW_MAX_US )); then
    echo "error: remote node did not complete successful ESB exchange" >&2
    exit 1
  fi
fi

if (( ((REMOTE_ANCHOR_RAW_WORD0 >> 24) & 0xFF) != SESSION_TAG )); then
  echo "error: remote node did not observe the expected session tag" >&2
  exit 1
fi

if [[ "${MODE}" == "baseline" ]]; then
  if (( LOCAL_ANCHORS_SUPPRESSED != 0 || LOCAL_FRAME_SEQUENCE_GAPS != 0 || LOCAL_FRAME_RECOVERY_EVENTS != 0 || LOCAL_FRAME_MISSING_COUNT != 0 || REMOTE_FRAMES_SUPPRESSED != 0 || REMOTE_ANCHOR_SEQUENCE_GAPS != 0 || REMOTE_ANCHOR_RECOVERY_EVENTS != 0 || REMOTE_ANCHOR_MISSING_COUNT != 0 )); then
    echo "error: ${MODE} mode unexpectedly observed anchor suppression or recovery" >&2
    exit 1
  fi
elif [[ "${MODE}" == "characterize" ]]; then
  if (( LOCAL_ANCHORS_SUPPRESSED != 0 || REMOTE_FRAMES_SUPPRESSED != 0 )); then
    echo "error: characterize mode started with unexpected suppression" >&2
    exit 1
  fi
elif [[ "${MODE}" == "blackout" ]]; then
  if (( LOCAL_ANCHORS_SUPPRESSED == 0 || REMOTE_ANCHOR_SEQUENCE_GAPS == 0 || REMOTE_ANCHOR_RECOVERY_EVENTS == 0 || REMOTE_ANCHOR_MISSING_COUNT < MASTER_BLACKOUT_LENGTH || REMOTE_MAX_ANCHOR_MASTER_DELTA_US < EXPECTED_BLACKOUT_GAP_MIN_US || REMOTE_MAX_ANCHOR_LOCAL_DELTA_US < EXPECTED_BLACKOUT_GAP_MIN_US )); then
    echo "error: blackout mode did not prove anchor loss and recovery" >&2
    exit 1
  fi
else
  if (( REMOTE_FRAMES_SUPPRESSED == 0 || LOCAL_FRAME_SEQUENCE_GAPS == 0 || LOCAL_FRAME_RECOVERY_EVENTS == 0 || LOCAL_FRAME_MISSING_COUNT < NODE_TX_BLACKOUT_LENGTH )); then
    echo "error: dropout mode did not prove frame loss and recovery" >&2
    exit 1
  fi
fi

if [[ "${MODE}" == "characterize" ]]; then
  mkdir -p "$(dirname "${SOAK_CSV}")"
  printf 'sample,elapsed_ms,local_rx_packets,local_rx_delta,local_anchor_seq,local_frame_gaps,local_frame_gap_delta,remote_tx_success,remote_tx_success_delta,remote_tx_failed,remote_anchors_received,remote_anchor_delta,remote_anchor_seq,remote_anchor_seq_gaps,remote_anchor_gap_delta,remote_offset_us,remote_skew_us,remote_skew_min_us,remote_skew_max_us,remote_max_master_delta_us,remote_max_local_delta_us\n' > "${SOAK_CSV}"
  printf '0,0,%d,0,%d,%d,0,%d,0,%d,%d,0,%d,%d,0,%d,%d,%d,%d,%d,%d\n' \
    "${LOCAL_RX_PACKETS}" "${LOCAL_LAST_ANCHOR_SEQUENCE}" "${LOCAL_FRAME_SEQUENCE_GAPS}" \
    "${REMOTE_TX_SUCCESS}" "${REMOTE_TX_FAILED}" "${REMOTE_ANCHORS_RECEIVED}" \
    "${REMOTE_LAST_ANCHOR_SEQUENCE}" "${REMOTE_ANCHOR_SEQUENCE_GAPS}" \
    "${REMOTE_ESTIMATED_OFFSET_US}" "${REMOTE_LAST_ANCHOR_SKEW_US}" \
    "${REMOTE_ANCHOR_SKEW_MIN_US}" "${REMOTE_ANCHOR_SKEW_MAX_US}" \
    "${REMOTE_MAX_ANCHOR_MASTER_DELTA_US}" "${REMOTE_MAX_ANCHOR_LOCAL_DELTA_US}" >> "${SOAK_CSV}"

  PREV_LOCAL_RX_PACKETS="${LOCAL_RX_PACKETS}"
  PREV_REMOTE_TX_SUCCESS="${REMOTE_TX_SUCCESS}"
  PREV_REMOTE_TX_FAILED="${REMOTE_TX_FAILED}"
  PREV_REMOTE_ANCHORS_RECEIVED="${REMOTE_ANCHORS_RECEIVED}"
  PREV_LOCAL_FRAME_SEQUENCE_GAPS="${LOCAL_FRAME_SEQUENCE_GAPS}"
  PREV_REMOTE_ANCHOR_SEQUENCE_GAPS="${REMOTE_ANCHOR_SEQUENCE_GAPS}"
  TOTAL_LOCAL_RX_DELTA=0
  TOTAL_REMOTE_TX_SUCCESS_DELTA=0
  TOTAL_REMOTE_TX_FAILED_DELTA=0
  TOTAL_REMOTE_ANCHOR_DELTA=0
  TOTAL_LOCAL_FRAME_GAP_DELTA=0
  TOTAL_REMOTE_ANCHOR_GAP_DELTA=0
  CHARACTERIZE_OFFSET_MIN_US="${REMOTE_ESTIMATED_OFFSET_US}"
  CHARACTERIZE_OFFSET_MAX_US="${REMOTE_ESTIMATED_OFFSET_US}"
  CHARACTERIZE_SKEW_MIN_US="${REMOTE_LAST_ANCHOR_SKEW_US}"
  CHARACTERIZE_SKEW_MAX_US="${REMOTE_LAST_ANCHOR_SKEW_US}"
  CHARACTERIZE_MAX_MASTER_DELTA_US="${REMOTE_MAX_ANCHOR_MASTER_DELTA_US}"
  CHARACTERIZE_MAX_LOCAL_DELTA_US="${REMOTE_MAX_ANCHOR_LOCAL_DELTA_US}"

  STATUS_CAPTURE_DELAY_MS="${SOAK_INTERVAL_MS}"
  for (( sample = 1; sample <= SOAK_SAMPLES; ++sample )); do
    echo "== characterize sample ${sample}/${SOAK_SAMPLES} =="
    capture_status_words "${LOCAL_STATUS_FILE}" LOCAL_WORDS run_local_status_live || true
    capture_status_words "${REMOTE_STATUS_FILE}" REMOTE_WORDS run_remote_status_live || true
    if [[ ${#LOCAL_WORDS[@]} -lt 39 || ${#REMOTE_WORDS[@]} -lt 39 ]]; then
      echo "error: characterize mode could not parse full status blocks" >&2
      exit 1
    fi
    decode_status_words

    if (( LOCAL_ROLE != 1 || LOCAL_PHASE != 4 || REMOTE_ROLE != 2 || REMOTE_PHASE != 4 || LOCAL_ANCHORS_SUPPRESSED != 0 || REMOTE_FRAMES_SUPPRESSED != 0 )); then
      echo "error: characterize mode observed invalid role/phase state or suppression" >&2
      exit 1
    fi

    LOCAL_RX_DELTA=$((LOCAL_RX_PACKETS - PREV_LOCAL_RX_PACKETS))
    REMOTE_TX_SUCCESS_DELTA=$((REMOTE_TX_SUCCESS - PREV_REMOTE_TX_SUCCESS))
    REMOTE_TX_FAILED_DELTA=$((REMOTE_TX_FAILED - PREV_REMOTE_TX_FAILED))
    REMOTE_ANCHOR_DELTA=$((REMOTE_ANCHORS_RECEIVED - PREV_REMOTE_ANCHORS_RECEIVED))
    LOCAL_FRAME_GAP_DELTA=$((LOCAL_FRAME_SEQUENCE_GAPS - PREV_LOCAL_FRAME_SEQUENCE_GAPS))
    REMOTE_ANCHOR_GAP_DELTA=$((REMOTE_ANCHOR_SEQUENCE_GAPS - PREV_REMOTE_ANCHOR_SEQUENCE_GAPS))

    if (( LOCAL_RX_DELTA <= 0 || REMOTE_TX_SUCCESS_DELTA <= 0 || REMOTE_ANCHOR_DELTA <= 0 )); then
      echo "error: characterize mode did not make forward RF progress during sample ${sample}" >&2
      exit 1
    fi

    printf 'sample=%d elapsed_ms=%d local_rx_delta=%d remote_tx_success_delta=%d remote_anchor_delta=%d local_frame_gap_delta=%d remote_anchor_gap_delta=%d offset_us=%d skew_us=%d\n' \
      "${sample}" "$((sample * SOAK_INTERVAL_MS))" "${LOCAL_RX_DELTA}" \
      "${REMOTE_TX_SUCCESS_DELTA}" "${REMOTE_ANCHOR_DELTA}" \
      "${LOCAL_FRAME_GAP_DELTA}" "${REMOTE_ANCHOR_GAP_DELTA}" \
      "${REMOTE_ESTIMATED_OFFSET_US}" "${REMOTE_LAST_ANCHOR_SKEW_US}"
    printf '%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n' \
      "${sample}" "$((sample * SOAK_INTERVAL_MS))" \
      "${LOCAL_RX_PACKETS}" "${LOCAL_RX_DELTA}" "${LOCAL_LAST_ANCHOR_SEQUENCE}" "${LOCAL_FRAME_SEQUENCE_GAPS}" "${LOCAL_FRAME_GAP_DELTA}" \
      "${REMOTE_TX_SUCCESS}" "${REMOTE_TX_SUCCESS_DELTA}" "${REMOTE_TX_FAILED}" \
      "${REMOTE_ANCHORS_RECEIVED}" "${REMOTE_ANCHOR_DELTA}" "${REMOTE_LAST_ANCHOR_SEQUENCE}" \
      "${REMOTE_ANCHOR_SEQUENCE_GAPS}" "${REMOTE_ANCHOR_GAP_DELTA}" "${REMOTE_ESTIMATED_OFFSET_US}" \
      "${REMOTE_LAST_ANCHOR_SKEW_US}" "${REMOTE_ANCHOR_SKEW_MIN_US}" \
      "${REMOTE_ANCHOR_SKEW_MAX_US}" "${REMOTE_MAX_ANCHOR_MASTER_DELTA_US}" \
      "${REMOTE_MAX_ANCHOR_LOCAL_DELTA_US}" >> "${SOAK_CSV}"

    TOTAL_LOCAL_RX_DELTA=$((TOTAL_LOCAL_RX_DELTA + LOCAL_RX_DELTA))
    TOTAL_REMOTE_TX_SUCCESS_DELTA=$((TOTAL_REMOTE_TX_SUCCESS_DELTA + REMOTE_TX_SUCCESS_DELTA))
    TOTAL_REMOTE_TX_FAILED_DELTA=$((TOTAL_REMOTE_TX_FAILED_DELTA + REMOTE_TX_FAILED_DELTA))
    TOTAL_REMOTE_ANCHOR_DELTA=$((TOTAL_REMOTE_ANCHOR_DELTA + REMOTE_ANCHOR_DELTA))
    TOTAL_LOCAL_FRAME_GAP_DELTA=$((TOTAL_LOCAL_FRAME_GAP_DELTA + LOCAL_FRAME_GAP_DELTA))
    TOTAL_REMOTE_ANCHOR_GAP_DELTA=$((TOTAL_REMOTE_ANCHOR_GAP_DELTA + REMOTE_ANCHOR_GAP_DELTA))
    if (( REMOTE_ESTIMATED_OFFSET_US < CHARACTERIZE_OFFSET_MIN_US )); then
      CHARACTERIZE_OFFSET_MIN_US="${REMOTE_ESTIMATED_OFFSET_US}"
    fi
    if (( REMOTE_ESTIMATED_OFFSET_US > CHARACTERIZE_OFFSET_MAX_US )); then
      CHARACTERIZE_OFFSET_MAX_US="${REMOTE_ESTIMATED_OFFSET_US}"
    fi
    if (( REMOTE_LAST_ANCHOR_SKEW_US < CHARACTERIZE_SKEW_MIN_US )); then
      CHARACTERIZE_SKEW_MIN_US="${REMOTE_LAST_ANCHOR_SKEW_US}"
    fi
    if (( REMOTE_LAST_ANCHOR_SKEW_US > CHARACTERIZE_SKEW_MAX_US )); then
      CHARACTERIZE_SKEW_MAX_US="${REMOTE_LAST_ANCHOR_SKEW_US}"
    fi
    if (( REMOTE_MAX_ANCHOR_MASTER_DELTA_US > CHARACTERIZE_MAX_MASTER_DELTA_US )); then
      CHARACTERIZE_MAX_MASTER_DELTA_US="${REMOTE_MAX_ANCHOR_MASTER_DELTA_US}"
    fi
    if (( REMOTE_MAX_ANCHOR_LOCAL_DELTA_US > CHARACTERIZE_MAX_LOCAL_DELTA_US )); then
      CHARACTERIZE_MAX_LOCAL_DELTA_US="${REMOTE_MAX_ANCHOR_LOCAL_DELTA_US}"
    fi

    PREV_LOCAL_RX_PACKETS="${LOCAL_RX_PACKETS}"
    PREV_REMOTE_TX_SUCCESS="${REMOTE_TX_SUCCESS}"
    PREV_REMOTE_TX_FAILED="${REMOTE_TX_FAILED}"
    PREV_REMOTE_ANCHORS_RECEIVED="${REMOTE_ANCHORS_RECEIVED}"
    PREV_LOCAL_FRAME_SEQUENCE_GAPS="${LOCAL_FRAME_SEQUENCE_GAPS}"
    PREV_REMOTE_ANCHOR_SEQUENCE_GAPS="${REMOTE_ANCHOR_SEQUENCE_GAPS}"
  done

  TOTAL_REMOTE_TX_ATTEMPTS=$((TOTAL_REMOTE_TX_SUCCESS_DELTA + TOTAL_REMOTE_TX_FAILED_DELTA))
  TOTAL_LOCAL_FRAMES_OBSERVED=$((TOTAL_LOCAL_RX_DELTA + TOTAL_LOCAL_FRAME_GAP_DELTA))
  TOTAL_REMOTE_ANCHORS_OBSERVED=$((TOTAL_REMOTE_ANCHOR_DELTA + TOTAL_REMOTE_ANCHOR_GAP_DELTA))
  if (( TOTAL_REMOTE_TX_ATTEMPTS > 0 )); then
    CHARACTERIZE_TX_FAIL_PPM=$((TOTAL_REMOTE_TX_FAILED_DELTA * 1000000 / TOTAL_REMOTE_TX_ATTEMPTS))
  else
    CHARACTERIZE_TX_FAIL_PPM=0
  fi
  if (( TOTAL_LOCAL_FRAMES_OBSERVED > 0 )); then
    CHARACTERIZE_FRAME_GAP_PPM=$((TOTAL_LOCAL_FRAME_GAP_DELTA * 1000000 / TOTAL_LOCAL_FRAMES_OBSERVED))
  else
    CHARACTERIZE_FRAME_GAP_PPM=0
  fi
  if (( TOTAL_REMOTE_ANCHORS_OBSERVED > 0 )); then
    CHARACTERIZE_ANCHOR_GAP_PPM=$((TOTAL_REMOTE_ANCHOR_GAP_DELTA * 1000000 / TOTAL_REMOTE_ANCHORS_OBSERVED))
  else
    CHARACTERIZE_ANCHOR_GAP_PPM=0
  fi

  cat > "${SOAK_SUMMARY}" <<EOF
session_tag=${SESSION_TAG}
samples=${SOAK_SAMPLES}
interval_ms=${SOAK_INTERVAL_MS}
total_elapsed_ms=$((SOAK_SAMPLES * SOAK_INTERVAL_MS))
total_local_rx_delta=${TOTAL_LOCAL_RX_DELTA}
total_remote_tx_success_delta=${TOTAL_REMOTE_TX_SUCCESS_DELTA}
total_remote_tx_failed_delta=${TOTAL_REMOTE_TX_FAILED_DELTA}
total_remote_anchor_delta=${TOTAL_REMOTE_ANCHOR_DELTA}
total_local_frame_gap_delta=${TOTAL_LOCAL_FRAME_GAP_DELTA}
total_remote_anchor_gap_delta=${TOTAL_REMOTE_ANCHOR_GAP_DELTA}
tx_fail_ppm=${CHARACTERIZE_TX_FAIL_PPM}
frame_gap_ppm=${CHARACTERIZE_FRAME_GAP_PPM}
anchor_gap_ppm=${CHARACTERIZE_ANCHOR_GAP_PPM}
offset_min_us=${CHARACTERIZE_OFFSET_MIN_US}
offset_max_us=${CHARACTERIZE_OFFSET_MAX_US}
skew_min_us=${CHARACTERIZE_SKEW_MIN_US}
skew_max_us=${CHARACTERIZE_SKEW_MAX_US}
max_master_delta_us=${CHARACTERIZE_MAX_MASTER_DELTA_US}
max_local_delta_us=${CHARACTERIZE_MAX_LOCAL_DELTA_US}
EOF

  printf 'characterize summary: tx_fail_ppm=%d frame_gap_ppm=%d anchor_gap_ppm=%d offset_range_us=[%d,%d] skew_range_us=[%d,%d] max_master_delta_us=%d max_local_delta_us=%d\n' \
    "${CHARACTERIZE_TX_FAIL_PPM}" "${CHARACTERIZE_FRAME_GAP_PPM}" "${CHARACTERIZE_ANCHOR_GAP_PPM}" \
    "${CHARACTERIZE_OFFSET_MIN_US}" "${CHARACTERIZE_OFFSET_MAX_US}" \
    "${CHARACTERIZE_SKEW_MIN_US}" "${CHARACTERIZE_SKEW_MAX_US}" \
    "${CHARACTERIZE_MAX_MASTER_DELTA_US}" "${CHARACTERIZE_MAX_LOCAL_DELTA_US}"

  echo "characterize csv: ${SOAK_CSV}"
  echo "characterize summary file: ${SOAK_SUMMARY}"
fi

echo "split-host ProPico ESB smoke (${MODE}): PASS"
