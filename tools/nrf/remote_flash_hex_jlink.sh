#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ $# -lt 4 || $# -gt 6 ]]; then
  echo "usage: $0 <user@host> <hex_relpath> <jlink_serial> <device> [reset_type] [remote_repo_dir]" >&2
  exit 2
fi

REMOTE="$1"
HEX_RELPATH="$2"
JLINK_SERIAL="$3"
DEVICE="$4"
RESET_TYPE="${5:-2}"
REMOTE_DIR="${6:-/home/litu/sandbox/embedded/HelixDrift}"

"${REPO_ROOT}/tools/dev/sync_remote_workspace.sh" "${REMOTE}" "${REMOTE_DIR}"

ssh "${REMOTE}" "cd ${REMOTE_DIR@Q} && ./tools/nrf/flash_hex_jlink.sh ${HEX_RELPATH@Q} ${JLINK_SERIAL@Q} ${DEVICE@Q} ${RESET_TYPE@Q}"
