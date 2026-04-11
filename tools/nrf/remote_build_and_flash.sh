#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ $# -lt 4 || $# -gt 6 ]]; then
  echo "usage: $0 <user@host> <target> <jlink_serial> <device> [reset_type] [remote_repo_dir]" >&2
  echo "example: $0 litu@hpserver1 nrf52840propico_bringup 123456 NRF52840_XXAA 0 ~/sandbox/embedded/HelixDrift" >&2
  exit 2
fi

REMOTE="$1"
TARGET="$2"
JLINK_SERIAL="$3"
DEVICE="$4"
RESET_TYPE="${5:-2}"
REMOTE_DIR="${6:-~/sandbox/embedded/HelixDrift}"

"${REPO_ROOT}/tools/dev/sync_remote_workspace.sh" "${REMOTE}" "${REMOTE_DIR}"

ssh "${REMOTE}" "cd ${REMOTE_DIR@Q} && nix develop --command bash -lc './tools/nrf/build_and_flash_jlink.sh ${TARGET@Q} ${JLINK_SERIAL@Q} ${DEVICE@Q} ${RESET_TYPE@Q}'"
