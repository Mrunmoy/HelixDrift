#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

ROLE="${1:-central}"
BOARD="${2:-nrf52840dongle/nrf52840/bare}"
NODE_ID="${3:-1}"
EXTRA_ARGS=("${@:4}")
EXTRA_CONF_APPEND="${HELIX_MOCAP_EXTRA_CONF_FILE:-}"

case "${ROLE}" in
  central)
    EXTRA_CONF="central.conf"
    ;;
  node)
    EXTRA_CONF="node.conf"
    ;;
  *)
    echo "usage: $0 <central|node> [board] [node_id]" >&2
    exit 2
    ;;
esac

NCS_VERSION="${NCS_VERSION:-v3.2.4}"
WORKSPACE_DIR="${NCS_WORKSPACE_DIR:-${REPO_ROOT}/.deps/ncs/${NCS_VERSION}}"
APP_DIR="${REPO_ROOT}/zephyr_apps/nrf52840-mocap-bridge"
BUILD_DIR_NAME="build-helix-${BOARD//\//-}-mocap-${ROLE}-node${NODE_ID}"
BOARD_OVERLAY="${APP_DIR}/boards/${BOARD//\//_}.overlay"
MCUBOOT_PATCH="${REPO_ROOT}/patches/0001-mcuboot-nrf-cleanup-gpio-present-check.patch"

"${REPO_ROOT}/tools/dev/bootstrap_ncs_workspace.sh" "${NCS_VERSION}" "${WORKSPACE_DIR}"

# Apply MCUboot GPIO safety patch for ProPico if needed
if [[ "${BOARD}" == promicro_nrf52840/* && "${ROLE}" == "node" ]]; then
  MCUBOOT_REPO="${WORKSPACE_DIR}/bootloader/mcuboot"
  MCUBOOT_TARGET_FILE="${MCUBOOT_REPO}/boot/zephyr/nrf_cleanup.c"
  if ! grep -q "nrfy_gpio_pin_present_check" "${MCUBOOT_TARGET_FILE}"; then
    git -C "${MCUBOOT_REPO}" apply "${MCUBOOT_PATCH}"
  fi
fi

export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH
GNUARMEMB_TOOLCHAIN_PATH="$(dirname "$(dirname "$(command -v arm-none-eabi-gcc)")")"

(
  cd "${WORKSPACE_DIR}"
  EXTRA_CONF_FILE="${APP_DIR}/${EXTRA_CONF}"
  if [[ -n "${EXTRA_CONF_APPEND}" ]]; then
    EXTRA_CONF_FILE="${EXTRA_CONF_FILE};${APP_DIR}/${EXTRA_CONF_APPEND}"
  fi

  BUILD_ARGS=(-DEXTRA_CONF_FILE="${EXTRA_CONF_FILE}")
  if [[ "${ROLE}" == "node" ]]; then
    BUILD_ARGS+=("-DCONFIG_HELIX_MOCAP_NODE_ID=${NODE_ID}")
  fi
  if [[ -f "${BOARD_OVERLAY}" ]]; then
    BUILD_ARGS+=("-DDTC_OVERLAY_FILE=${BOARD_OVERLAY}")
  fi
  # Node builds use sysbuild for MCUboot signing; central builds don't need MCUboot
  SYSBUILD_FLAG=()
  if [[ "${ROLE}" == "node" ]]; then
    SYSBUILD_FLAG=(--sysbuild)
  fi

  west build ${SYSBUILD_FLAG[@]+"${SYSBUILD_FLAG[@]}"} -p auto -b "${BOARD}" "${APP_DIR}" -d "${BUILD_DIR_NAME}" -- \
    "${BUILD_ARGS[@]}" \
    "${EXTRA_ARGS[@]}"
)
