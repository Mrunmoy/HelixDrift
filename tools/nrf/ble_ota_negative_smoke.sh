#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PORT="${1:-/dev/ttyACM0}"
TARGET_CFG="${2:-target/nrf52.cfg}"

cd "${REPO_ROOT}"

read_console_until() {
  local needle="$1"
  python3 - "$PORT" "$needle" <<'PY'
import os, sys, time, termios, fcntl
port = sys.argv[1]
needle = sys.argv[2]
deadline = time.time() + 15.0
TIOCMBIS = 0x5416
TIOCM_DTR = 0x002
fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
try:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    fcntl.ioctl(fd, TIOCMBIS, int(TIOCM_DTR).to_bytes(4, "little"))
    buf = ""
    while time.time() < deadline:
        try:
            chunk = os.read(fd, 4096).decode("utf-8", "replace")
        except BlockingIOError:
            chunk = ""
        if chunk:
            buf += chunk
            if needle in buf:
                print(buf)
                raise SystemExit(0)
        time.sleep(0.1)
    print(buf)
    raise SystemExit(1)
finally:
    os.close(fd)
PY
}

reset_target() {
  openocd \
    -c "adapter driver jlink; transport select swd; source [find ${TARGET_CFG}]; init; reset run; shutdown"
}

tools/dev/doctor.sh
tools/nrf/build_helix_ble_ota.sh v1
tools/nrf/build_helix_ble_ota.sh v2

tools/nrf/recover_openocd.sh "${TARGET_CFG}"
tools/nrf/flash_openocd.sh \
  .deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v1/merged.hex \
  "${TARGET_CFG}"

IMAGE=".deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v2/nrf52dk-ota-ble/zephyr/zephyr.signed.bin"

echo "== bad CRC must not reboot into v2 =="
if python3 tools/nrf/ble_ota_upload.py \
  "${IMAGE}" \
  --name HelixOTA-v1 \
  --expect-after HelixOTA-v2 \
  --crc-adjust 1 \
  --chunk-size 16 \
  --poll-every-chunks 64 \
  --inter-chunk-delay-ms 1; then
  echo "unexpected success for bad CRC OTA" >&2
  exit 1
fi
read_console_until "tick ota-ble-v1 state=0 bytes=166548"
reset_target

python3 tools/nrf/ble_ota_upload.py \
  "${IMAGE}" \
  --name HelixOTA-v1 \
  --abort-after-bytes 4096 \
  --chunk-size 16 \
  --poll-every-chunks 32 \
  --inter-chunk-delay-ms 1 \
  --no-wait-after
read_console_until "tick ota-ble-v1 state=0 bytes=0"
reset_target

echo "== final good update must still work =="
python3 tools/nrf/ble_ota_upload.py \
  "${IMAGE}" \
  --name HelixOTA-v1 \
  --expect-after HelixOTA-v2 \
  --chunk-size 16 \
  --poll-every-chunks 64 \
  --inter-chunk-delay-ms 1
