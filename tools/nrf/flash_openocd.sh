#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <image.hex|image.bin> [target_cfg]" >&2
  echo "example: $0 build/nrf/nrf52dk_blinky.hex target/nrf52.cfg" >&2
  exit 2
fi

IMAGE="$1"
TARGET_CFG="${2:-target/nrf52.cfg}"
JLINK_SERIAL="${JLINK_SERIAL:-}"

if [[ ! -f "$IMAGE" ]]; then
  echo "error: image not found: $IMAGE" >&2
  exit 1
fi

OPENOCD_CMDS="adapter driver jlink; "
if [[ -n "${JLINK_SERIAL}" ]]; then
  OPENOCD_CMDS+="adapter serial ${JLINK_SERIAL}; "
fi
OPENOCD_CMDS+="transport select swd; source [find ${TARGET_CFG}]; init; program ${IMAGE} verify reset exit"

exec openocd -c "${OPENOCD_CMDS}"
