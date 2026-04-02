#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

TARGET_CFG="${1:-target/nrf52.cfg}"

exec openocd \
  -c "adapter driver jlink; transport select swd; source [find ${TARGET_CFG}]; init; halt; nrf5 mass_erase; reset run; shutdown"
