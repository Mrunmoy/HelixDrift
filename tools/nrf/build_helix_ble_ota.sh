#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

VARIANT="${1:-v1}"
BOARD="${2:-nrf52dk/nrf52832}"
NCS_VERSION="${NCS_VERSION:-v3.2.4}"
WORKSPACE_DIR="${NCS_WORKSPACE_DIR:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"
APP_DIR="${REPO_ROOT}/zephyr_apps/nrf52dk-ota-ble"
BUILD_BOARD_SLUG="${BOARD//\//-}"
BUILD_DIR_NAME="build-helix-${BUILD_BOARD_SLUG}-ota-ble-${VARIANT}"

case "${BOARD}" in
  nrf52dk/nrf52832)
    EXTRA_CONF_FILE="${APP_DIR}/${VARIANT}.conf"
    ;;
  nrf52840dongle/nrf52840|nrf52840dongle/nrf52840/bare)
    EXTRA_CONF_FILE="${APP_DIR}/${VARIANT}-nrf52840dongle.conf"
    ;;
  *)
    echo "unsupported OTA BLE board: ${BOARD}" >&2
    exit 2
    ;;
esac

case "${VARIANT}" in
  v1|v2) ;;
  *)
    echo "usage: $0 [v1|v2] [board]" >&2
    exit 2
    ;;
esac

"${REPO_ROOT}/tools/dev/bootstrap_ncs_workspace.sh" "${NCS_VERSION}" "${WORKSPACE_DIR}"

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH
GNUARMEMB_TOOLCHAIN_PATH="$(dirname "$(dirname "$(command -v arm-none-eabi-gcc)")")"

(
  cd "${WORKSPACE_DIR}"
  west build --sysbuild -p auto -b "${BOARD}" "${APP_DIR}" -d "${BUILD_DIR_NAME}" \
    -- -DEXTRA_CONF_FILE="${EXTRA_CONF_FILE}"
)
