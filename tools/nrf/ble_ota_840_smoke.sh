#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TARGET_CFG="${1:-target/nrf52.cfg}"
export JLINK_SERIAL="${JLINK_SERIAL:-4294967295}"

cd "${REPO_ROOT}"

tools/dev/doctor.sh
tools/nrf/build_helix_ble_ota.sh v1 nrf52840dongle/nrf52840/bare
tools/nrf/build_helix_ble_ota.sh v2 nrf52840dongle/nrf52840/bare

tools/nrf/recover_openocd.sh "${TARGET_CFG}"
tools/nrf/flash_openocd.sh \
  .deps/ncs/v3.2.4/build-helix-nrf52840dongle-nrf52840-bare-ota-ble-v1/merged.hex \
  "${TARGET_CFG}"

python3 tools/nrf/ble_ota_upload.py \
  .deps/ncs/v3.2.4/build-helix-nrf52840dongle-nrf52840-bare-ota-ble-v2/nrf52dk-ota-ble/zephyr/zephyr.signed.bin \
  --name Helix840-v1 \
  --expect-after Helix840-v2 \
  --target-id 0x52840059 \
  --chunk-size 16 \
  --poll-every-chunks 64 \
  --inter-chunk-delay-ms 1
