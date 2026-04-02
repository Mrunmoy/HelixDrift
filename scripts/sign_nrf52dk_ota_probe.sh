#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-nrf52dk_ota_probe_v1}"
VERSION="${2:-1.0.0+0}"
OUT_BASE="${3:-${REPO_ROOT}/build/nrf/${TARGET}_signed}"

APP_HEX="${REPO_ROOT}/build/nrf/${TARGET}.hex"
APP_BIN="${REPO_ROOT}/build/nrf/${TARGET}.bin"
KEY_PEM="${REPO_ROOT}/keys/dev_signing_key.pem"
SLOT_SIZE="0x24000"

if [[ ! -f "${APP_HEX}" || ! -f "${APP_BIN}" ]]; then
  echo "error: missing build artifacts for ${TARGET}; run ./build.py --nrf-only first" >&2
  exit 1
fi

"${REPO_ROOT}/scripts/sign_firmware.sh" \
  "${APP_HEX}" \
  "${KEY_PEM}" \
  "${VERSION}" \
  "${OUT_BASE}.hex" \
  "${SLOT_SIZE}"

"${REPO_ROOT}/tools/imgtool.sh" sign \
  --key "${KEY_PEM}" \
  --align 4 \
  --version "${VERSION}" \
  --header-size 0x200 \
  --slot-size "${SLOT_SIZE}" \
  --pad-header \
  "${APP_BIN}" \
  "${OUT_BASE}.bin"

echo "Signed DK OTA target:"
echo "  HEX: ${OUT_BASE}.hex"
echo "  BIN: ${OUT_BASE}.bin"
