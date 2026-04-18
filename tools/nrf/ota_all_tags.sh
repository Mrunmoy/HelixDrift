#!/usr/bin/env bash
# OTA all Tags through the Hub relay.
#
# Workflow (per Tag):
#   1. Physically reset the Tag (unplug/replug USB, or J-Link reset if
#      the Tag is on a J-Link probe).
#   2. Within 30 seconds, run this script with the Tag's BLE name.
#      Each OTA takes ~46 seconds.
#
# The Tag's BLE name is HTag-XXXX, where XXXX is the last 4 hex digits
# of its FICR device address.  To find the name, reset the Tag and scan
# BLE from a phone, or use tools/nrf/ble_scan.py.
#
# Usage:
#   ./ota_all_tags.sh <image.signed.bin> <HTag-XXXX> [HTag-YYYY ...]
#
# Example:
#   ./ota_all_tags.sh zephyr.signed.bin HTag-0D16 HTag-C489 HTag-EFDB ...

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <image.signed.bin> <HTag-XXXX> [HTag-YYYY ...]" >&2
    exit 1
fi

IMAGE="$1"
shift
TARGETS=("$@")

if [[ ! -f "$IMAGE" ]]; then
    echo "ERROR: image not found: $IMAGE" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
UPLOADER="$SCRIPT_DIR/hub_ota_upload.py"
PORT="${HUB_PORT:-/dev/ttyACM1}"

echo "=== Helix multi-Tag OTA via Hub relay ==="
echo "Image: $IMAGE"
echo "Hub port: $PORT"
echo "Targets: ${TARGETS[*]}"
echo

for i in "${!TARGETS[@]}"; do
    n=$((i + 1))
    total=${#TARGETS[@]}
    target="${TARGETS[$i]}"

    echo "--- [$n/$total] OTA to $target ---"
    echo "STEP 1: Physically reset $target now (unplug/replug USB)."
    echo "STEP 2: Press Enter within 30 seconds to start OTA..."
    read -r

    start=$(date +%s)
    if python3 "$UPLOADER" "$IMAGE" --port "$PORT" --target "$target"; then
        elapsed=$(($(date +%s) - start))
        echo "--- [$n/$total] $target done in ${elapsed}s ---"
    else
        elapsed=$(($(date +%s) - start))
        echo "--- [$n/$total] $target FAILED after ${elapsed}s ---"
        echo "Press Enter to continue with next Tag, or Ctrl-C to abort..."
        read -r
    fi
    echo
done

echo "=== All Tags processed ==="
