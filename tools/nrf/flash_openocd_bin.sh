#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "usage: $0 <image.bin> <flash_addr> [target_cfg]" >&2
  echo "example: $0 build/nrf/nrf52dk_ota_probe_v2_signed.bin 0x00038000 target/nrf52.cfg" >&2
  exit 2
fi

IMAGE="$1"
FLASH_ADDR="$2"
TARGET_CFG="${3:-target/nrf52.cfg}"

if [[ ! -f "$IMAGE" ]]; then
  echo "error: image not found: $IMAGE" >&2
  exit 1
fi

exec openocd \
  -c "adapter driver jlink; transport select swd; source [find ${TARGET_CFG}]; init; program ${IMAGE} ${FLASH_ADDR} verify reset exit"
