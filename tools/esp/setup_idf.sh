#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IDF_DIR="${ROOT_DIR}/third_party/esp-idf"

cd "${ROOT_DIR}"
git submodule update --init third_party/esp-idf

if [[ ! -x "${IDF_DIR}/install.sh" ]]; then
  echo "ESP-IDF install.sh not found at ${IDF_DIR}" >&2
  exit 1
fi

# Installs tools for ESP32-S3 target (idempotent).
"${IDF_DIR}/install.sh" esp32s3
