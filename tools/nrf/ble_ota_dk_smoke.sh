#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PORT="${1:-/dev/ttyACM0}"
TARGET_CFG="${2:-target/nrf52.cfg}"

cd "${REPO_ROOT}"

tools/dev/doctor.sh
tools/nrf/build_helix_ble_ota.sh v1
tools/nrf/build_helix_ble_ota.sh v2

tools/nrf/recover_openocd.sh "${TARGET_CFG}"
tools/nrf/flash_openocd.sh \
  .deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v1/merged.hex \
  "${TARGET_CFG}"

python3 tools/nrf/ble_ota_upload.py \
  .deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v2/nrf52dk-ota-ble/zephyr/zephyr.signed.bin \
  --name HelixOTA-v1 \
  --expect-after HelixOTA-v2 \
  --chunk-size 16
