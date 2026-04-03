# nRF BLE OTA Workflow

This is the supported HelixDrift over-the-air update workflow for the current
`nrf-xiao-nrf52840` branch.

Current validated hardware:
- target board: `nRF52 DK (nRF52832)`
- BLE central/uploader: this Linux PC over `hci0`
- debug/flash path: onboard SEGGER J-Link via OpenOCD

This workflow is fully repo-local:
- clone with submodules
- enter `nix develop`
- use repo scripts only

No manual installation of:
- Nordic Connect SDK
- Zephyr SDK
- `nrfutil`
- `west`
- Python BLE packages

is required outside the repo + nix shell contract.

## What Is Proven

The following is already proven on real hardware:

1. `HelixOTA-v1` boots through MCUboot on the DK
2. the device advertises the Helix OTA BLE service
3. this PC uploads a signed `v2` image over BLE
4. the running app stages the image in slot 1
5. the app marks the upgrade pending
6. MCUboot promotes the image on reboot
7. the board comes back as `HelixOTA-v2`

Observed smoke result:

```text
before: F2:A5:1E:5F:5B:9C HelixOTA-v1
status-before: state=0 bytes=0 last=0 target=0x52832001
commit: OK, waiting for reboot
after: F2:A5:1E:5F:5B:9C HelixOTA-v2
```

The OTA service now also reports and enforces a target identifier:

- `0x52832001` = `nrf52dk/nrf52832`
- `0x52840059` = `nrf52840` dongle lane
- `0x52840040` = `XIAO nRF52840` product lane

An uploader must match the running image target ID before `BEGIN` is accepted.

## Prerequisites

```bash
git clone --recurse-submodules <repo-url>
cd HelixDrift
nix develop
```

Recommended first check:

```bash
tools/dev/doctor.sh
```

## OTA Components

Bootloader:
- vendored MCUboot

Target app:
- [`zephyr_apps/nrf52dk-ota-ble`](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52dk-ota-ble)

Build helper:
- [`tools/nrf/build_helix_ble_ota.sh`](/home/mrumoy/sandbox/embedded/HelixDrift/tools/nrf/build_helix_ble_ota.sh)

Uploader:
- [`tools/nrf/ble_ota_upload.py`](/home/mrumoy/sandbox/embedded/HelixDrift/tools/nrf/ble_ota_upload.py)

End-to-end smoke:
- [`tools/nrf/ble_ota_dk_smoke.sh`](/home/mrumoy/sandbox/embedded/HelixDrift/tools/nrf/ble_ota_dk_smoke.sh)

## Build The OTA Images

Build `v1`:

```bash
nix develop --command bash -lc 'tools/nrf/build_helix_ble_ota.sh v1'
```

Build `v2`:

```bash
nix develop --command bash -lc 'tools/nrf/build_helix_ble_ota.sh v2'
```

Outputs:

- initial flash image:
  - `.deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v1/merged.hex`
- OTA payload:
  - `.deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v2/nrf52dk-ota-ble/zephyr/zephyr.signed.bin`

## First-Time Flash

Recover and flash the target with `v1`:

```bash
nix develop --command bash -lc '
  tools/nrf/recover_openocd.sh target/nrf52.cfg &&
  tools/nrf/flash_openocd.sh \
    .deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v1/merged.hex \
    target/nrf52.cfg
'
```

Optional live serial check:

```bash
picocom -b 115200 /dev/ttyACM0
```

Expected lines include:

```text
helix ota ble boot: ota-ble-v1
mcuboot: image confirmed
ble: advertising as HelixOTA-v1
tick ota-ble-v1 state=0 bytes=0
```

## Perform OTA Over BLE

Upload the signed `v2` payload:

```bash
nix develop --command python3 tools/nrf/ble_ota_upload.py \
  .deps/ncs/v3.2.4/build-helix-nrf52dk-ota-ble-v2/nrf52dk-ota-ble/zephyr/zephyr.signed.bin \
  --name HelixOTA-v1 \
  --expect-after HelixOTA-v2 \
  --target-id 0x52832001
```

Current safe default:
- chunk size defaults to `16`

That default is intentionally conservative because it is the currently proven
stable chunk size on this DK path.

## One-Command Smoke

Use this when you want the full repo-supported proof path:

```bash
nix develop --command bash -lc 'tools/nrf/ble_ota_dk_smoke.sh /dev/ttyACM0 target/nrf52.cfg'
```

This will:
1. run `tools/dev/doctor.sh`
2. build `v1`
3. build `v2`
4. recover the DK
5. flash `v1`
6. upload `v2` over BLE
7. wait for reboot into `HelixOTA-v2`

## Failure-Handling Smoke

The repository also provides a negative-path smoke:

```bash
nix develop --command bash -lc 'tools/nrf/ble_ota_negative_smoke.sh /dev/ttyACM0 target/nrf52.cfg'
```

This validates four real-hardware cases:
1. bad CRC over BLE must not promote `v2`
2. wrong target ID must be rejected before the transfer begins
3. explicit abort mid-transfer must return the target to idle on `v1`
4. a later valid BLE OTA must still promote successfully to `v2`

Representative result:

```text
== bad CRC must not reboot into v2 ==
... ATT error: 0x13 (Value Not Allowed)
tick ota-ble-v1 state=0 bytes=166548

== wrong target id must be rejected immediately ==
error: wrong target id: device=0x52832001 expected=0x52840059

abort: OK after 4096 bytes
tick ota-ble-v1 state=0 bytes=0

== final good update must still work ==
after: F2:A5:1E:5F:5B:9C HelixOTA-v2
```

## Bootloader / Layout Notes

Current validated branch layout:
- bootloader: `96 KB`
- primary slot: `352 KB`
- secondary slot: `352 KB`
- scratch: `32 KB`
- NVS/calibration: `192 KB`

Reference:
- [`docs/NRF_OTA_MEMORY_PLAN.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF_OTA_MEMORY_PLAN.md)

Current boot policy:
- dual-slot
- MCUboot
- pending upgrade into secondary slot
- reboot into new image
- app confirms itself on healthy startup

## Supported Developer Contract

The supported contract for another developer is:

```bash
git clone --recurse-submodules <repo-url>
cd HelixDrift
nix develop
tools/dev/doctor.sh
tools/nrf/build_helix_ble_ota.sh v1
tools/nrf/build_helix_ble_ota.sh v2
tools/nrf/ble_ota_dk_smoke.sh /dev/ttyACM0 target/nrf52.cfg
```

If that sequence works, the OTA path is considered healthy.

## What Is Still Not Claimed

This workflow proves:
- repo-local buildability
- flashing
- BLE OTA transport
- MCUboot promotion

It does not yet prove:
- final `nRF52840` product board behavior
- attached production sensor stack behavior
- production-grade security policy beyond the current debug/test signing path

## Recommended Lock-In Policy

Treat this as the current supported OTA baseline:

- do not switch to one-slot OTA
- do not shrink slots yet
- do not raise BLE chunk size casually without revalidation
- do not bypass `tools/dev/doctor.sh`
- keep target-ID checks intact across all OTA transports
- keep the smoke script passing whenever OTA-related code changes
