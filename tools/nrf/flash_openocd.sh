#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <image.hex|image.bin> [target_cfg]" >&2
  echo "example: $0 build/nrf/nrf52dk_blinky.hex target/nrf52.cfg" >&2
  exit 2
fi

IMAGE="$1"
TARGET_CFG="${2:-target/nrf52.cfg}"

if [[ ! -f "$IMAGE" ]]; then
  echo "error: image not found: $IMAGE" >&2
  exit 1
fi

exec openocd \
  -c "adapter driver jlink; transport select swd; source [find ${TARGET_CFG}]; init; program ${IMAGE} verify reset exit"
