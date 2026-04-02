#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "usage: $0 <slot_base_hex> <slot_size_hex> [target_cfg]" >&2
  echo "example: $0 0x0003C000 0x00024000 target/nrf52.cfg" >&2
  exit 2
fi

SLOT_BASE="$1"
SLOT_SIZE="$2"
TARGET_CFG="${3:-target/nrf52.cfg}"

read -r TRAILER_ADDR <<EOF
$(python3 - <<'PY' "${SLOT_BASE}" "${SLOT_SIZE}"
import sys
base = int(sys.argv[1], 0)
size = int(sys.argv[2], 0)
print(hex(base + size - 28))
PY
)
EOF

TMP_BIN="$(mktemp /tmp/helix-mcuboot-trailer-XXXXXX.bin)"
trap 'rm -f "${TMP_BIN}"' EXIT

python3 - <<'PY' "${TMP_BIN}"
from pathlib import Path
payload = bytes([
    0x03, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0xFF, 0xFF, 0xFF,
    0x04, 0x00, 0x2D, 0xE1,
    0x5D, 0x29, 0x41, 0x0B,
    0x8D, 0x77, 0x67, 0x9C,
    0x11, 0x0F, 0x1F, 0x8A,
])
Path(__import__("sys").argv[1]).write_bytes(payload)
PY

exec openocd \
  -c "adapter driver jlink; transport select swd; source [find ${TARGET_CFG}]; init; program ${TMP_BIN} ${TRAILER_ADDR} verify reset exit"
