#!/usr/bin/env bash
# Usage: flash_tag.sh <jlink_sn> <node_id> [log_path] [hex_path]
#
# SWD-programs a Tag with the merged Hub-relay OTA firmware image,
# then writes the node_id byte into the first word of the
# settings_storage partition at 0xFE000. See docs/NRF_HUB_RELAY_OTA.md
# and docs/RF_CLOSEOUT_HANDOFF.md for the full provisioning story.
set -u
SN="${1:?need jlink serial number}"
NODE_ID="${2:?need node_id 1..255}"
ARTDIR="${HELIX_RF_ARTIFACT_DIR:-/tmp/helix_tag_log}"
LOG="${3:-${ARTDIR}/programmed.txt}"

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEFAULT_HEX="${REPO_ROOT}/.deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-mocap-node-node1/merged.hex"
HEX="${4:-${HELIX_TAG_MERGED_HEX:-${DEFAULT_HEX}}}"

mkdir -p "$(dirname "$LOG")"

if (( NODE_ID < 1 || NODE_ID > 255 )); then echo "node_id must be 1..255"; exit 2; fi
if [[ ! -f "$HEX" ]]; then echo "merged hex not found at $HEX — build the Tag firmware first"; exit 2; fi

echo "--- Programming node_id=${NODE_ID} via J-Link ${SN} ---"
nrfutil device program --firmware "$HEX" --serial-number "$SN" --options verify=VERIFY_READ 2>&1 | tail -2
# Provision node_id (flash was just erased, so page at 0xFE000 is all 0xFF)
VAL=$(printf "0xFFFFFF%02X" "$NODE_ID")
# NOTE: NO --direct flag — direct skips NVMC setup so a flash write is
# silently dropped. Plain `device write` uses NVMC and actually writes.
nrfutil device write --serial-number "$SN" --address 0xFE000 --value "$VAL" 2>&1 | tail -1
nrfutil device reset --serial-number "$SN" 2>&1 | tail -1
sleep 3
SLOT0=$(nrfutil device read --serial-number "$SN" --address 0xC014 --bytes 4 --direct 2>&1 | grep "0x0000C014" | awk '{print $2}')
CFG=$(nrfutil device read --serial-number "$SN" --address 0xFE000 --bytes 4 --direct 2>&1 | grep "0x000FE000" | awk '{print $2}')
FICR=$(nrfutil device read --serial-number "$SN" --address 0x100000A4 --bytes 4 --direct 2>&1 | grep "0x100000A4" | awk '{print $2}')
BYTE0=${FICR:6:2}; BYTE1=${FICR:4:2}
SUFFIX="${BYTE0:0:1}${BYTE0:1:1}${BYTE1:0:1}${BYTE1:1:1}"
NAME="HTag-${SUFFIX}"
# Extract stored node_id from config word (expect FFFFFFnn)
STORED_ID=$((0x${CFG:6:2}))
echo "node_id=${NODE_ID} (stored=${STORED_ID}) slot0=${SLOT0} cfg=${CFG} FICR=${FICR} name=${NAME}"
printf "node_id=%-3d  stored=%-3d  slot0=%s  cfg=%s  FICR=%s  name=%s\n" \
    "$NODE_ID" "$STORED_ID" "$SLOT0" "$CFG" "$FICR" "$NAME" >> "$LOG"
if [[ "$STORED_ID" -ne "$NODE_ID" ]]; then
    echo "!! MISMATCH: expected ${NODE_ID}, got ${STORED_ID}"
    exit 1
fi
