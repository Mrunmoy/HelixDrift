#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

NCS_VERSION="${1:-v3.2.4}"
WORKSPACE_DIR="${2:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"
MANIFEST_URL="${NCS_MANIFEST_URL:-https://github.com/nrfconnect/sdk-nrf.git}"

mkdir -p "$(dirname "${WORKSPACE_DIR}")"

if [[ ! -d "${WORKSPACE_DIR}/.west" ]]; then
  echo "Initializing NCS workspace at ${WORKSPACE_DIR} (${NCS_VERSION})"
  west init -m "${MANIFEST_URL}" --mr "${NCS_VERSION}" "${WORKSPACE_DIR}"
fi

echo "Updating NCS workspace at ${WORKSPACE_DIR}"
(
  cd "${WORKSPACE_DIR}"
  west update --narrow -o=--depth=1
)

echo "NCS workspace ready: ${WORKSPACE_DIR}"
