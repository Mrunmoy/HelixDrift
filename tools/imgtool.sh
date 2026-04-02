#!/usr/bin/env bash
set -euo pipefail

if ! command -v python3 >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMGTOOL="${REPO_ROOT}/third_party/mcuboot/scripts/imgtool.py"

if [[ ! -f "${IMGTOOL}" ]]; then
  echo "error: MCUboot imgtool not found at ${IMGTOOL}" >&2
  echo "hint: run 'git submodule update --init --recursive'" >&2
  exit 1
fi

if ! python3 -c "import click, cbor2, cryptography, intelhex" >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

exec python3 "${IMGTOOL}" "$@"
