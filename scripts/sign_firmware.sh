#!/usr/bin/env bash
# sign_firmware.sh — Sign a HelixDrift nRF52 application image with imgtool.
#
# Usage:
#   ./scripts/sign_firmware.sh [APP_HEX] [KEY_PEM] [VERSION] [OUTPUT_HEX]
#
# Defaults:
#   APP_HEX    build/nrf/nrf52_mocap_node.hex
#   KEY_PEM    keys/dev_signing_key.pem
#   VERSION    1.0.0+0
#   OUTPUT_HEX build/nrf/nrf52_mocap_node_signed.hex
#
# The signed image is padded to the primary slot size (0x58000 = 352 KB)
# and ready to flash at offset 0x00018000 (after the 96 KB MCUboot bootloader).
#
# Flash procedure (J-Link):
#   nrfjprog --program build/bootloader/nrf52_bootloader.hex --chiperase --verify --reset
#   nrfjprog --program build/nrf/nrf52_mocap_node_signed.hex --sectorerase --verify --reset

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

APP_HEX="${1:-${REPO_ROOT}/build/nrf/nrf52_mocap_node.hex}"
KEY_PEM="${2:-${REPO_ROOT}/keys/dev_signing_key.pem}"
VERSION="${3:-1.0.0+0}"
OUTPUT_HEX="${4:-${REPO_ROOT}/build/nrf/nrf52_mocap_node_signed.hex}"

# Flash layout constants (must match tools/linker/xiao_nrf52840_app.ld)
HEADER_SIZE="0x200"    # 512 B — MCUboot image header
SLOT_SIZE="0x58000"    # 352 KB — primary / secondary slot size
ALIGN="4"              # nRF52840 NVMC write alignment

echo "Signing: ${APP_HEX}"
echo "Key:     ${KEY_PEM}"
echo "Version: ${VERSION}"

"${REPO_ROOT}/tools/imgtool.sh" sign \
    --key        "${KEY_PEM}" \
    --align      "${ALIGN}" \
    --version    "${VERSION}" \
    --header-size "${HEADER_SIZE}" \
    --slot-size  "${SLOT_SIZE}" \
    --pad-header \
    "${APP_HEX}" \
    "${OUTPUT_HEX}"

echo "Signed image: ${OUTPUT_HEX}"

# Extract the public key if the installed imgtool emits a PEM-compatible format.
# Older MCUboot/imgtool revisions emit a C array instead, which is still useful
# for embedding in the bootloader but cannot be fed back into `imgtool verify`.
PUB_KEY="${KEY_PEM%.pem}_pub.pem"
"${REPO_ROOT}/tools/imgtool.sh" getpub --key "${KEY_PEM}" --output "${PUB_KEY}" 2>/dev/null || true

# Verify only when the generated public key is actually PEM.
if [ -f "${PUB_KEY}" ] && grep -q "BEGIN PUBLIC KEY" "${PUB_KEY}"; then
    "${REPO_ROOT}/tools/imgtool.sh" verify --key "${PUB_KEY}" "${OUTPUT_HEX}"
    echo "Signature verification: OK"
else
    echo "Signature verification: skipped (imgtool getpub did not emit PEM)"
fi
