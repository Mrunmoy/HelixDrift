#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

if [[ -f sources.env ]]; then
  # shellcheck disable=SC1091
  source sources.env
else
  echo "sources.env not found"
  exit 1
fi

fetch_pdf() {
  local url="$1"
  local out="$2"
  if [[ -z "${url}" ]]; then
    echo "skip ${out}: empty URL"
    return 0
  fi
  if [[ "${url}" != *.pdf ]]; then
    echo "skip ${out}: URL is not direct PDF (${url})"
    return 0
  fi
  echo "fetch ${out}"
  curl -L --fail --output "${out}" "${url}"
}

fetch_pdf "${NRF52840_PS_URL:-}" "nRF52840_Product_Specification.pdf"
fetch_pdf "${NRF52840_ERRATA_URL:-}" "nRF52840_Errata.pdf"
fetch_pdf "${ARM_CORTEX_M4_TRM_URL:-}" "ARM_Cortex_M4_TRM.pdf"
fetch_pdf "${ARMV7M_ARM_URL:-}" "ARMv7M_ARM.pdf"
fetch_pdf "${LSM6DSO_DS_URL:-}" "LSM6DSO_Datasheet.pdf"
fetch_pdf "${BMM350_DS_URL:-}" "BMM350_Datasheet.pdf"
fetch_pdf "${LPS22DF_DS_URL:-}" "LPS22DF_Datasheet.pdf"

echo "done"
