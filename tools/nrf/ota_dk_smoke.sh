#!/usr/bin/env bash
set -euo pipefail

if ! command -v openocd >/dev/null 2>&1; then
  exec nix develop --command bash -lc "$(printf '%q ' "$0" "$@")"
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PORT="${1:-/dev/ttyACM0}"
TARGET_CFG="${2:-target/nrf52.cfg}"

cd "${REPO_ROOT}"

./build.py --nrf-only
./build.py --bootloader
./scripts/sign_nrf52dk_ota_probe.sh nrf52dk_ota_probe_v1 1.0.0+0
./scripts/sign_nrf52dk_ota_probe.sh nrf52dk_ota_probe_v2 1.1.0+0

tools/nrf/mass_erase_openocd.sh "${TARGET_CFG}"
tools/nrf/flash_openocd.sh build/bootloader/nrf52dk_bootloader.hex "${TARGET_CFG}"
tools/nrf/flash_openocd.sh build/nrf/nrf52dk_ota_probe_v1_signed.hex "${TARGET_CFG}"

python3 - <<'PY' "${PORT}" "probe v1" 5
import os, sys, time, termios, fcntl
port, needle, timeout = sys.argv[1], sys.argv[2], float(sys.argv[3])
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
    fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NONBLOCK)
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        try:
            chunk = os.read(fd, 4096)
            if chunk:
                buf += chunk
                if needle.encode() in buf:
                    print(buf.decode(errors="replace"))
                    raise SystemExit(0)
        except BlockingIOError:
            pass
        time.sleep(0.05)
    print(buf.decode(errors="replace"))
    raise SystemExit(1)
finally:
    os.close(fd)
PY

tools/nrf/flash_openocd_bin.sh build/nrf/nrf52dk_ota_probe_v2_signed.bin 0x0003C000 "${TARGET_CFG}"
tools/nrf/write_mcuboot_pending_trailer.sh 0x0003C000 0x00024000 "${TARGET_CFG}"

python3 - <<'PY' "${PORT}" "probe v2" 8
import os, sys, time, termios, fcntl
port, needle, timeout = sys.argv[1], sys.argv[2], float(sys.argv[3])
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
    fcntl.fcntl(fd, fcntl.F_SETFL, os.O_NONBLOCK)
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        try:
            chunk = os.read(fd, 4096)
            if chunk:
                buf += chunk
                if needle.encode() in buf:
                    print(buf.decode(errors="replace"))
                    raise SystemExit(0)
        except BlockingIOError:
            pass
        time.sleep(0.05)
    print(buf.decode(errors="replace"))
    raise SystemExit(1)
finally:
    os.close(fd)
PY
