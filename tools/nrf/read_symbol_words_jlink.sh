#!/usr/bin/env bash
set -euo pipefail

if ! command -v arm-none-eabi-nm >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

if [[ $# -lt 4 || $# -gt 5 ]]; then
  echo "usage: $0 <elf> <symbol> <jlink_serial> <device> [words]" >&2
  exit 2
fi

ELF="$1"
SYMBOL="$2"
JLINK_SERIAL="$3"
DEVICE="$4"
WORDS="${5:-8}"

ADDR="$(arm-none-eabi-nm -n "$ELF" | awk -v sym="$SYMBOL" '$3==sym {print "0x"$1; exit}')"
if [[ -z "${ADDR}" ]]; then
  echo "error: symbol not found: ${SYMBOL}" >&2
  exit 1
fi

CMD_FILE="$(mktemp)"
trap 'rm -f "${CMD_FILE}"' EXIT
cat >"${CMD_FILE}" <<EOF
connect
halt
mem32 ${ADDR}, ${WORDS}
go
q
EOF

exec JLinkExe \
  -USB "${JLINK_SERIAL}" \
  -Device "${DEVICE}" \
  -If SWD \
  -Speed 1000 \
  -AutoConnect 1 \
  -NoGui 1 \
  -CommandFile "${CMD_FILE}"
