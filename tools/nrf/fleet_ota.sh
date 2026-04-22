#!/usr/bin/env bash
# Fleet Hub-relay OTA stress test.
#
# For each of N rounds, builds a Tag firmware with bumped version, then
# walks all 10 Tags sequentially and: resets Hub, ESB-triggers the Tag
# (so Tag reboots into the BLE OTA window), and OTAs via hub_ota_upload.
# Retries once on failure. Logs everything.
set -u
ROUNDS=${1:-3}
START_VER=${2:-2}
ARTDIR="${HELIX_RF_ARTIFACT_DIR:-/tmp/helix_tag_log}"
LOG="${ARTDIR}/fleet_ota.log"
SUMMARY="${ARTDIR}/fleet_ota_summary.txt"
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR=.deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-mocap-node-node1
NODE_CONF=zephyr_apps/nrf52840-mocap-bridge/node.conf
UPLOAD_SCRIPT=tools/nrf/hub_ota_upload.py
HUB_SN="${HELIX_HUB_JLINK_SN:-69656876}"
JLINK_SN="${HELIX_TAG_JLINK_SN:-123456}"   # Tag on J-Link-Plus (= HTag-C489, node_id=1)
HUB_TTY="${HELIX_HUB_TTY:-/dev/ttyACM1}"
mkdir -p "$ARTDIR"

TAGS=(
    "1:HTag-C489"
    "2:HTag-0D16"
    "3:HTag-8A49"
    "4:HTag-08F4"
    "5:HTag-0126"
    "6:HTag-817E"
    "7:HTag-25A7"
    "8:HTag-4F8D"
    "9:HTag-EFDB"
    "10:HTag-9895"
)

TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_ATTEMPTS=0
declare -a RESULT_LINES

cd "$REPO_ROOT"

log() { echo "$@" | tee -a "$LOG"; }

ota_tag_once() {
    # Returns 0/1, writes "STATUS|ELAPSED|OUTPUT" to stdout.
    local NID="$1" NAME="$2"
    local START OUT ELAPSED STATUS
    START=$(date +%s)
    nrfutil device reset --serial-number $HUB_SN >/dev/null 2>&1
    sleep 8
    OUT=$(timeout 360 python3 $UPLOAD_SCRIPT \
        $BUILD_DIR/nrf52840-mocap-bridge/zephyr/zephyr.signed.bin \
        --port "$HUB_TTY" --target "$NAME" --trigger-node $NID 2>&1)
    ELAPSED=$(( $(date +%s) - START ))
    if echo "$OUT" | grep -qE "Hub ESB restarted|COMMIT OK|COMMIT response timeout \(expected"; then
        STATUS="PASS"
    else
        STATUS="FAIL"
    fi
    printf '%s|%s|%s' "$STATUS" "$ELAPSED" "$OUT"
}

{
    echo "=== FLEET OTA TEST ==="
    echo "Started: $(date)"
    echo "Rounds:  $ROUNDS"
    echo "Tags:    10"
    echo "Targets: $((ROUNDS * 10)) OTAs"
    echo "Build:   v$START_VER .. v$((START_VER + ROUNDS - 1))"
    echo ""
} > "$LOG"

for (( R=0; R<ROUNDS; R++ )); do
    VER=$((START_VER + R))
    log ""
    log "=== ROUND $((R+1))/$ROUNDS  →  v${VER}.0.0  @ $(date +%T) ==="

    sed -i "s/^CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=.*/CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"${VER}.0.0+0\"/" $NODE_CONF
    log "Building firmware v$VER..."
    BUILD_START=$(date +%s)
    # Move any prior artifact aside BEFORE starting the new build. Keeps a
    # ".last-good" copy for operator recovery, while ensuring a mid-build
    # Ctrl+C or compile failure can't silently ship stale bits (bit us on
    # v19 — see commit c7697da). Copilot code review round 7.
    ARTIFACT_SIGNED="$BUILD_DIR/nrf52840-mocap-bridge/zephyr/zephyr.signed.bin"
    ARTIFACT_MERGED="$BUILD_DIR/merged.hex"
    [ -f "$ARTIFACT_SIGNED" ] && mv "$ARTIFACT_SIGNED" "$ARTIFACT_SIGNED.last-good"
    [ -f "$ARTIFACT_MERGED" ] && mv "$ARTIFACT_MERGED" "$ARTIFACT_MERGED.last-good"
    nix develop --command bash -lc "CCACHE_DISABLE=1 tools/nrf/build_mocap_bridge.sh node promicro_nrf52840/nrf52840 1" >> "$LOG" 2>&1
    BUILD_RC=$?
    if [ $BUILD_RC -ne 0 ] || [ ! -f "$ARTIFACT_SIGNED" ]; then
        log "!! BUILD FAILED for v$VER (exit $BUILD_RC) — aborting round"
        log "   previous artifact preserved at $ARTIFACT_SIGNED.last-good"
        continue
    fi
    log "Build done in $(( $(date +%s) - BUILD_START ))s"

    for TAG in "${TAGS[@]}"; do
        NID="${TAG%%:*}"
        NAME="${TAG##*:}"
        TOTAL_ATTEMPTS=$((TOTAL_ATTEMPTS + 1))
        log ""
        log "-- R$((R+1)).$NID $NAME → v$VER (attempt 1/2) --"

        RESULT=$(ota_tag_once "$NID" "$NAME")
        STATUS="${RESULT%%|*}"
        REST="${RESULT#*|}"
        ELAPSED="${REST%%|*}"
        OUT="${REST#*|}"
        echo "$OUT" | tail -5 >> "$LOG"
        log "  attempt 1: $STATUS in ${ELAPSED}s"

        if [ "$STATUS" = "FAIL" ]; then
            log "  retrying..."
            RESULT=$(ota_tag_once "$NID" "$NAME")
            STATUS="${RESULT%%|*}"
            REST="${RESULT#*|}"
            R_ELAPSED="${REST%%|*}"
            R_OUT="${REST#*|}"
            echo "$R_OUT" | tail -5 >> "$LOG"
            log "  attempt 2: $STATUS in ${R_ELAPSED}s"
            ELAPSED="$R_ELAPSED"
        fi

        if [ "$STATUS" = "PASS" ]; then
            TOTAL_PASS=$((TOTAL_PASS + 1))
        else
            TOTAL_FAIL=$((TOTAL_FAIL + 1))
        fi
        RESULT_LINES+=("R$((R+1)) v$VER  nid=$NID  $NAME  $STATUS  ${ELAPSED}s")

        # Whenever node_id=1 is the target, also verify slot0 via SWD
        if [ "$NID" = "1" ]; then
            sleep 4
            SLOT0=$(nrfutil device read --serial-number $JLINK_SN --address 0xC014 --bytes 4 --direct 2>&1 | grep 0xC014 | awk '{print $2}')
            EXPECTED=$(printf "%08X" "$VER")
            if [ "$SLOT0" = "$EXPECTED" ]; then
                log "  [J-Link verify] slot0=$SLOT0 matches v$VER ✓"
            else
                log "  [J-Link verify] slot0=$SLOT0 expected $EXPECTED  ✗"
            fi
        fi
    done

    log ""
    log "-- Round $((R+1)) done: running PASS=$TOTAL_PASS FAIL=$TOTAL_FAIL of $TOTAL_ATTEMPTS --"
done

{
    echo ""
    echo "================================================================"
    echo "FLEET OTA TEST COMPLETE"
    echo "Ended: $(date)"
    echo "Attempts: $TOTAL_ATTEMPTS"
    echo "PASS:     $TOTAL_PASS"
    echo "FAIL:     $TOTAL_FAIL"
    if [ $TOTAL_ATTEMPTS -gt 0 ]; then
        echo "Rate:     $(( TOTAL_PASS * 100 / TOTAL_ATTEMPTS ))%"
    fi
    echo ""
    echo "Per-Tag results:"
    printf '  %s\n' "${RESULT_LINES[@]}"
} | tee "$SUMMARY" >> "$LOG"
