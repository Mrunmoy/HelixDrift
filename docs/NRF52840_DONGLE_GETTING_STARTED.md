# nRF52840 Dongle Getting Started

This is the current bring-up and OTA guide for the soldered
`nrf52840dongle/nrf52840/bare` lane on `nrf-xiao-nrf52840`.

It documents what is already usable today and what is still not fully closed.

## Current State

Already proven on real hardware:

- repo-native `nrf52840dongle_blinky` builds and runs
- SWD flashing works through a J-Link
- step-debug works on the dongle
- the repo-local Zephyr/MCUboot BLE OTA lane builds for
  `nrf52840dongle/nrf52840/bare`
- the dongle boots and advertises `Helix840-v1`
- the `nRF52840` OTA target ID is distinct:
  - `0x52840059`

Not yet fully closed:

- a clean, repeatable `Helix840-v1 -> Helix840-v2` BLE OTA success on the
  dongle
- the same proof on the second `nRF52840` target

For the exact checkpoint state, see
[`docs/NRF52840_BLE_OTA_CHECKPOINT.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF52840_BLE_OTA_CHECKPOINT.md).

## Requirements

Supported developer contract:

```bash
git clone --recurse-submodules <repo-url>
cd HelixDrift
nix develop
tools/dev/doctor.sh
```

No separate manual install of:

- Nordic Connect SDK
- Zephyr SDK
- `west`
- `nrfutil`
- Python BLE packages

is required outside the repo + nix shell.

## Hardware Assumptions

- target: `nRF52840` dongle
- probe: J-Link over SWD
- current OpenOCD target config:
  - `target/nrf52.cfg`
- if more than one J-Link is attached, select the probe explicitly:
  - `JLINK_SERIAL=<serial>`

Example used during current bring-up:

```bash
export JLINK_SERIAL=4294967295
```

## Build The Dongle OTA Images

Build `v1`:

```bash
nix develop --command bash -lc 'tools/nrf/build_helix_ble_ota.sh v1 nrf52840dongle/nrf52840/bare'
```

Build `v2`:

```bash
nix develop --command bash -lc 'tools/nrf/build_helix_ble_ota.sh v2 nrf52840dongle/nrf52840/bare'
```

Outputs:

- first-flash image:
  - `.deps/ncs/v3.2.4/build-helix-nrf52840dongle-nrf52840-bare-ota-ble-v1/merged.hex`
- OTA payload:
  - `.deps/ncs/v3.2.4/build-helix-nrf52840dongle-nrf52840-bare-ota-ble-v2/nrf52dk-ota-ble/zephyr/zephyr.signed.bin`

## First-Time Flash

Recover and flash `Helix840-v1`:

```bash
nix develop --command bash -lc '
  JLINK_SERIAL=${JLINK_SERIAL:-4294967295} tools/nrf/recover_openocd.sh target/nrf52.cfg &&
  JLINK_SERIAL=${JLINK_SERIAL:-4294967295} tools/nrf/flash_openocd.sh \
    .deps/ncs/v3.2.4/build-helix-nrf52840dongle-nrf52840-bare-ota-ble-v1/merged.hex \
    target/nrf52.cfg
'
```

## Verify The Dongle Is Alive

Current expected BLE name after first flash:

- `Helix840-v1`

Quick scan from this PC:

```bash
nix develop --command bash -lc 'python3 - <<'"'"'PY'"'"'
import asyncio
from bleak import BleakScanner
async def main():
    devs = await BleakScanner.discover(timeout=10.0, return_adv=True)
    for addr,(dev,adv) in devs.items():
        name = dev.name or adv.local_name or ""
        if "Helix840" in name:
            print(addr, name)
asyncio.run(main())
PY'
```
```

Expected result includes:

```text
<BLE_ADDR> Helix840-v1
```

## Current BLE OTA Command

This is the current direct uploader command being used for the 52840 lane:

```bash
nix develop --command bash -lc '
  python3 -u tools/nrf/ble_ota_upload.py \
    .deps/ncs/v3.2.4/build-helix-nrf52840dongle-nrf52840-bare-ota-ble-v2/nrf52dk-ota-ble/zephyr/zephyr.signed.bin \
    --name Helix840-v1 \
    --expect-after Helix840-v2 \
    --target-id 0x52840059 \
    --chunk-size 64 \
    --write-with-response \
    --poll-every-chunks 4 \
    --inter-chunk-delay-ms 0 \
    --progress-every-bytes 8192 \
    --resume-retries 2 \
    --tail-safe-bytes 4096 \
    --tail-chunk-size 4 \
    --tail-write-with-response \
    --tail-poll-every-chunks 1 \
    --tail-inter-chunk-delay-ms 5 \
    --page-cross-delay-ms 200 \
    --gatt-timeout 10 \
    --begin-timeout 60 \
    --commit-timeout 30
'
```

## Important Note About The 52840 OTA Lane

Do not treat the `nRF52840` BLE OTA lane as fully locked yet.

What is true:

- build path works
- first flash works
- advertising works
- `BEGIN` timeout behavior is understood
- transfer can progress on the real target

What is not yet true:

- a clean, repeatable, documented final `Helix840-v1 -> Helix840-v2` proof on
  the dongle

Until that is closed, use this guide as a bring-up/reference path, not as the
final product OTA guide.

## Related Docs

- [`docs/NRF_BLE_OTA_WORKFLOW.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF_BLE_OTA_WORKFLOW.md)
- [`docs/M7_NRF_BRINGUP_CHECKLIST.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/M7_NRF_BRINGUP_CHECKLIST.md)
- [`docs/NRF52840_BLE_OTA_CHECKPOINT.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF52840_BLE_OTA_CHECKPOINT.md)
