#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if ! command -v cmake >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

cd "${REPO_ROOT}"

required_tools=(
  git
  python3
  pytest
  cmake
  ninja
  arm-none-eabi-gcc
  arm-none-eabi-objcopy
  arm-none-eabi-size
  openocd
  west
  pyocd
  dfu-util
  picocom
  nc
)

echo "Checking required tools..."
for tool in "${required_tools[@]}"; do
  if ! command -v "${tool}" >/dev/null 2>&1; then
    echo "missing tool: ${tool}" >&2
    exit 1
  fi
done

echo "Checking Python analysis dependencies..."
python3 - <<'PY'
import click
import cbor2
import cryptography
import intelhex
import matplotlib
import numpy
import pydantic
import pytest
print("python deps OK")
PY

echo "Checking submodules..."
git submodule update --init external/SensorFusion third_party/mcuboot
git submodule status

echo "Checking MCUboot imgtool wrapper..."
tools/imgtool.sh version >/dev/null

echo "Doctor OK"
