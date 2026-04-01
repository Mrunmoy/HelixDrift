#!/usr/bin/env bash
set -euo pipefail

if ! command -v arm-none-eabi-nm >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

if [[ $# -lt 2 || $# -gt 4 ]]; then
  echo "usage: $0 <elf> <symbol> [words] [target_cfg]" >&2
  exit 2
fi

ELF="$1"
SYMBOL="$2"
WORDS="${3:-8}"
TARGET_CFG="${4:-target/nrf52.cfg}"

if [[ ! -f "$ELF" ]]; then
  echo "error: elf not found: $ELF" >&2
  exit 1
fi

ADDR="$(arm-none-eabi-nm -n "$ELF" | awk -v sym="$SYMBOL" '$3==sym {print "0x"$1; exit}')"
if [[ -z "${ADDR}" ]]; then
  echo "error: symbol not found: $SYMBOL" >&2
  exit 1
fi

exec openocd \
  -c "adapter driver jlink; transport select swd; source [find ${TARGET_CFG}]; init; halt; mdw ${ADDR} ${WORDS}; resume; exit"
