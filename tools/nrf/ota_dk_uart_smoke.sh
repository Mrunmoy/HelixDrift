#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PORT="${1:-/dev/ttyACM0}"
TARGET_CFG="${2:-target/nrf52.cfg}"

cd "${REPO_ROOT}"

./build.py --nrf-only
./build.py --bootloader
./scripts/sign_nrf52dk_ota_probe.sh nrf52dk_ota_serial_v1 1.0.0+0
./scripts/sign_nrf52dk_ota_probe.sh nrf52dk_ota_serial_v2 1.1.0+0

tools/nrf/mass_erase_openocd.sh "${TARGET_CFG}"
tools/nrf/flash_openocd.sh build/bootloader/nrf52dk_bootloader.hex "${TARGET_CFG}"
tools/nrf/flash_openocd.sh build/nrf/nrf52dk_ota_serial_v1_signed.hex "${TARGET_CFG}"

python3 tools/nrf/uart_ota_upload.py \
  "${PORT}" \
  build/nrf/nrf52dk_ota_serial_v2_signed.bin \
  --expect-before ota-v1 \
  --expect-after ota-v2
