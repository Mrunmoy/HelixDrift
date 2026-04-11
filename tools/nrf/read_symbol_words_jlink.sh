#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 4 || $# -gt 6 ]]; then
  echo "usage: $0 <elf> <symbol> <jlink_serial> <device> [words] [settle_ms]" >&2
  exit 2
fi

ELF="$1"
SYMBOL="$2"
JLINK_SERIAL="$3"
DEVICE="$4"
WORDS="${5:-8}"
SETTLE_MS="${6:-0}"

if command -v arm-none-eabi-nm >/dev/null 2>&1; then
  ADDR="$(arm-none-eabi-nm -n "$ELF" | awk -v sym="$SYMBOL" '$3==sym {print "0x"$1; exit}')"
else
  ADDR="$(nix develop --command bash -lc "arm-none-eabi-nm -n '$(printf "%s" "$ELF")' | awk -v sym='$(printf "%s" "$SYMBOL")' '\$3==sym {print \"0x\"\$1; exit}'")"
fi
if [[ -z "${ADDR}" ]]; then
  echo "error: symbol not found: ${SYMBOL}" >&2
  exit 1
fi

CMD_FILE="$(mktemp)"
trap 'rm -f "${CMD_FILE}"' EXIT
cat >"${CMD_FILE}" <<EOF
connect
EOF

if (( SETTLE_MS > 0 )); then
  cat >>"${CMD_FILE}" <<EOF
r
g
sleep ${SETTLE_MS}
halt
EOF
else
  cat >>"${CMD_FILE}" <<EOF
halt
EOF
fi

cat >>"${CMD_FILE}" <<EOF
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
