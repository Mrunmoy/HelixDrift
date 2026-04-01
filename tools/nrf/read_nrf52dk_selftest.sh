#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "usage: $0 <elf> [target_cfg]" >&2
  echo "example: $0 build/nrf/nrf52dk_selftest target/nrf52.cfg" >&2
  exit 2
fi

ELF="$1"
TARGET_CFG="${2:-target/nrf52.cfg}"

if [[ ! -f "$ELF" ]]; then
  echo "error: elf not found: $ELF" >&2
  exit 1
fi

if ! command -v arm-none-eabi-nm >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

STATUS_ADDR="$(arm-none-eabi-nm -n "$ELF" | awk '/g_selfTestStatus/ {print "0x"$1; exit}')"
if [[ -z "${STATUS_ADDR}" ]]; then
  echo "error: could not locate g_selfTestStatus in $ELF" >&2
  exit 1
fi

exec openocd \
  -c "adapter driver jlink; transport select swd; source [find ${TARGET_CFG}]; init; halt; mdw ${STATUS_ADDR} 6; mdw 0x0007e000 4; mdw 0x0007effc 1; mdw 0x0007f000 4; resume; exit"
