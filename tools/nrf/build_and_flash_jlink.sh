#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ $# -lt 3 || $# -gt 4 ]]; then
  echo "usage: $0 <target> <jlink_serial> <device> [reset_type]" >&2
  echo "example: $0 nrf52dk_bringup 1050335103 NRF52832_XXAA 2" >&2
  exit 2
fi

TARGET="$1"
JLINK_SERIAL="$2"
DEVICE="$3"
RESET_TYPE="${4:-2}"
IMAGE="${REPO_ROOT}/build/nrf/${TARGET}.hex"

(
  cd "${REPO_ROOT}"
  ./build.py --nrf-only
)

if [[ ! -f "${IMAGE}" ]]; then
  echo "error: expected image not found after build: ${IMAGE}" >&2
  exit 1
fi

CMD_FILE="$(mktemp)"
trap 'rm -f "${CMD_FILE}"' EXIT

cat >"${CMD_FILE}" <<EOF
connect
rsettype ${RESET_TYPE}
r
loadfile ${IMAGE}
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
