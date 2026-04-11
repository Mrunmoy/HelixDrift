#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 || $# -gt 4 ]]; then
  echo "usage: $0 <hex> <jlink_serial> <device> [reset_type]" >&2
  exit 2
fi

HEX="$1"
JLINK_SERIAL="$2"
DEVICE="$3"
RESET_TYPE="${4:-2}"

if [[ ! -f "${HEX}" ]]; then
  echo "error: hex not found: ${HEX}" >&2
  exit 1
fi

CMD_FILE="$(mktemp)"
trap 'rm -f "${CMD_FILE}"' EXIT

cat >"${CMD_FILE}" <<EOF
connect
rsettype ${RESET_TYPE}
r
loadfile ${HEX}
r
g
q
EOF

exec JLinkExe \
  -USB "${JLINK_SERIAL}" \
  -Device "${DEVICE}" \
  -If SWD \
  -Speed 1000 \
  -AutoConnect 1 \
  -NoGui 1 \
  -CommandFile "${CMD_FILE}"
