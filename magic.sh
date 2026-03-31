#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

echo "[1/2] Running host tests"
./build.py --host-only -t

echo "[2/2] Building nRF project"
./build.py --nrf-only

echo "Done."
