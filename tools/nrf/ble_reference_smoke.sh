#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v west >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

SERIAL_PORT="${1:-/dev/ttyACM0}"
TARGET_CFG="${2:-target/nrf52.cfg}"
MESSAGE="${3:-HELIX_BLE_SMOKE}"

cd "${REPO_ROOT}"

tools/nrf/build_ncs_sample.sh nrf/samples/bluetooth/peripheral_uart nrf52dk/nrf52832 build-nrf52dk-nrf52832-peripheral_uart
tools/nrf/recover_openocd.sh "${TARGET_CFG}"
tools/nrf/flash_openocd.sh .deps/ncs/v3.2.4/build-nrf52dk-nrf52832-peripheral_uart/merged.hex "${TARGET_CFG}"
python3 tools/nrf/ble_nus_smoke.py --serial-port "${SERIAL_PORT}" --message "${MESSAGE}"
