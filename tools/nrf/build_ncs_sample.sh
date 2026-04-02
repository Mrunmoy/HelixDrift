#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

SAMPLE_REL="${1:-nrf/samples/bluetooth/peripheral_uart}"
BOARD="${2:-nrf52dk/nrf52832}"
BUILD_DIR_NAME="${3:-build-${BOARD//\//-}-$(basename "${SAMPLE_REL}")}"
NCS_VERSION="${NCS_VERSION:-v3.2.4}"
WORKSPACE_DIR="${NCS_WORKSPACE_DIR:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"

"${REPO_ROOT}/tools/dev/bootstrap_ncs_workspace.sh" "${NCS_VERSION}" "${WORKSPACE_DIR}"

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH
GNUARMEMB_TOOLCHAIN_PATH="$(dirname "$(dirname "$(command -v arm-none-eabi-gcc)")")"

(
  cd "${WORKSPACE_DIR}"
  west build -p always -b "${BOARD}" "${SAMPLE_REL}" -d "${BUILD_DIR_NAME}"
)
