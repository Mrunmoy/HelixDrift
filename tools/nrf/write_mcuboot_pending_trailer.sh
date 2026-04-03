#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "usage: $0 <slot_base_hex> <slot_size_hex> [target_cfg]" >&2
  echo "example: $0 0x00085000 0x00079000 target/nrf52.cfg" >&2
  exit 2
fi

SLOT_BASE="$1"
SLOT_SIZE="$2"
TARGET_CFG="${3:-target/nrf52.cfg}"
JLINK_SERIAL="${JLINK_SERIAL:-}"

read -r SWAP_INFO_ADDR MAGIC_ADDR <<EOF
$(python3 - <<'PY' "${SLOT_BASE}" "${SLOT_SIZE}"
import sys
base = int(sys.argv[1], 0)
size = int(sys.argv[2], 0)
end = base + size
print(hex(end - 40), hex(end - 16))
PY
)
EOF

TMP_SWAP="$(mktemp /tmp/helix-mcuboot-swapinfo-XXXXXX.bin)"
TMP_MAGIC="$(mktemp /tmp/helix-mcuboot-magic-XXXXXX.bin)"
trap 'rm -f "${TMP_SWAP}" "${TMP_MAGIC}"' EXIT

python3 - <<'PY' "${TMP_SWAP}" "${TMP_MAGIC}"
from pathlib import Path
swap_payload = bytes([
    0x02, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,
])
magic_payload = bytes([
    0x77, 0xC2, 0x95, 0xF3,
    0x60, 0xD2, 0xEF, 0x7F,
    0x35, 0x52, 0x50, 0x0F,
    0x2C, 0xB6, 0x79, 0x80,
])
Path(__import__("sys").argv[1]).write_bytes(swap_payload)
Path(__import__("sys").argv[2]).write_bytes(magic_payload)
PY

OPENOCD_CMDS="adapter driver jlink; "
if [[ -n "${JLINK_SERIAL}" ]]; then
  OPENOCD_CMDS+="adapter serial ${JLINK_SERIAL}; "
fi
OPENOCD_CMDS+="transport select swd; source [find ${TARGET_CFG}]; init; program ${TMP_SWAP} ${SWAP_INFO_ADDR} verify; program ${TMP_MAGIC} ${MAGIC_ADDR} verify reset exit"

exec openocd -c "${OPENOCD_CMDS}"
