#!/usr/bin/env bash
set -euo pipefail

UF2_FILE="${1:?usage: $0 <uf2-file> <mountpoint>}"
MOUNTPOINT="${2:?usage: $0 <uf2-file> <mountpoint>}"

if [[ ! -f "${UF2_FILE}" ]]; then
  echo "uf2 file not found: ${UF2_FILE}" >&2
  exit 2
fi

if [[ ! -d "${MOUNTPOINT}" ]]; then
  echo "mountpoint not found: ${MOUNTPOINT}" >&2
  exit 2
fi

dest="${MOUNTPOINT}/$(basename "${UF2_FILE}")"
cp "${UF2_FILE}" "${dest}"
sync
echo "flashed ${UF2_FILE} -> ${dest}"
