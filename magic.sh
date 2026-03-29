#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

echo "[1/3] Bootstrapping ESP-IDF toolchain"
bash tools/esp/setup_idf.sh

echo "[2/3] Running host tests"
./build.py --host-only -t

echo "[3/3] Building ESP32-S3 project"
./build.py --esp32s3-only

echo "Done."
