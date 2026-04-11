#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

ROLE="${1:-master}"
BOARD="${2:-promicro_nrf52840/nrf52840/uf2}"
NODE_ID="${3:-1}"

case "${ROLE}" in
  master)
    EXTRA_CONF="master.conf"
    ;;
  node)
    EXTRA_CONF="node.conf"
    ;;
  *)
    echo "usage: $0 <master|node> [board] [node_id]" >&2
    exit 2
    ;;
esac

NCS_VERSION="${NCS_VERSION:-v3.2.4}"
WORKSPACE_DIR="${NCS_WORKSPACE_DIR:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"
APP_DIR="${REPO_ROOT}/zephyr_apps/nrf52840propico-esb-link"
BUILD_DIR_NAME="build-helix-${BOARD//\//-}-esb-${ROLE}-node${NODE_ID}"

"${REPO_ROOT}/tools/dev/bootstrap_ncs_workspace.sh" "${NCS_VERSION}" "${WORKSPACE_DIR}"

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH
GNUARMEMB_TOOLCHAIN_PATH="$(dirname "$(dirname "$(command -v arm-none-eabi-gcc)")")"

(
  cd "${WORKSPACE_DIR}"
  west build -p auto -b "${BOARD}" "${APP_DIR}" -d "${BUILD_DIR_NAME}" -- \
    -DEXTRA_CONF_FILE="${APP_DIR}/${EXTRA_CONF}" \
    -DCONFIG_HELIX_ESB_NODE_ID="${NODE_ID}"
)
