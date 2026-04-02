# nRF BLE Reference Workflow

This document defines the repo-supported way to build a Nordic/Zephyr BLE
reference app from a plain clone of this repository.

Goal:
- no manual NCS installation
- no manual Zephyr SDK installation
- all host tools available from `nix develop`
- the Nordic SDK workspace is bootstrapped by repo-local scripts when needed

## Scope

This workflow is for:
- proving the local machine can build a BLE-capable nRF reference app
- validating BLE transport assumptions before those pieces are fully absorbed
  into HelixDrift
- reducing developer setup friction for future OTA-over-BLE work

It now also includes a repo-local Helix BLE OTA lane for the connected
`nrf52dk/nrf52832`.

## Prerequisites

```bash
git clone --recurse-submodules <repo>
cd HelixDrift
nix develop
```

The `nix develop` shell provides:
- `west`
- `cmake`
- `ninja`
- `arm-none-eabi-gcc`
- `dtc`
- `gperf`
- `bluez`

## Bootstrap The Nordic Workspace

The repo can bootstrap a pinned Nordic Connect SDK workspace under
`.deps/ncs/`.

```bash
tools/dev/bootstrap_ncs_workspace.sh
```

Default pinned version:
- `v3.2.4`

Default bootstrap location:
- `.deps/ncs/v3.2.4`

This keeps Nordic dependencies outside the tracked tree while preserving a
repo-local, reproducible path for any developer.

Behavior:
- first run initializes and updates the workspace
- later runs reuse the existing workspace by default
- force a refresh with `HELIX_NCS_REFRESH=1`

## Build A BLE Reference Sample

The current reference sample is Nordic's `peripheral_uart` on the `nrf52dk`.

```bash
tools/nrf/build_ncs_sample.sh
```

Defaults:
- sample: `nrf/samples/bluetooth/peripheral_uart`
- board: `nrf52dk/nrf52832`
- pristine mode: `auto`

Build output:
- `.deps/ncs/v3.2.4/build-nrf52dk-nrf52832-peripheral_uart/`

Force a pristine rebuild if needed:

```bash
HELIX_NCS_PRISTINE=always tools/nrf/build_ncs_sample.sh
```

## Flashing

Once built, flash the resulting hex with the repo-local OpenOCD flow:

```bash
tools/nrf/flash_openocd.sh \
  .deps/ncs/v3.2.4/build-nrf52dk-nrf52832-peripheral_uart/merged.hex \
  target/nrf52.cfg
```

## Over-The-Air Smoke

The repo also provides a smoke path that:
- builds the Nordic UART Service sample
- recovers the DK if APPROTECT is active
- flashes it to the connected DK
- scans for the BLE peripheral from this PC
- writes a test payload over BLE
- verifies that payload reaches the DK serial console

```bash
nix develop --command bash -lc 'tools/nrf/ble_reference_smoke.sh /dev/ttyACM0 target/nrf52.cfg'
```

Supporting tool:
- `tools/nrf/ble_nus_smoke.py`
- `tools/nrf/recover_openocd.sh`

## Why This Exists

HelixDrift already proves:
- MCUboot OTA stability
- real transport OTA over UART/VCOM
- nRF52 DK flashing, VCOM, and board bring-up

What remains open is real BLE transport. This workflow proves the build and
host environment needed for that BLE lane without relying on a separately
installed personal NCS setup.

## Current Status

Validated in this branch:
- `west` BLE sample build works from `nix develop`
- nix-provided GNU Arm toolchain is accepted by the Nordic/Zephyr build
- missing host utilities previously observed (`dtc`, `gperf`) are now expected
  from the nix shell
- the PC can discover the flashed DK over BLE and deliver a payload over the
  Nordic UART Service, with the payload observed on the DK serial console
- the repo-local recover + flash + BLE smoke path is reproducible with:
  `tools/nrf/recover_openocd.sh`, `tools/nrf/flash_openocd.sh`,
  and `tools/nrf/ble_nus_smoke.py`

Still open:
- attached-sensor bring-up on real hardware
- later RF sanity checks on additional Nordic targets

## Helix BLE OTA On DK

The repository now includes a real Helix BLE OTA target app:

- `zephyr_apps/nrf52dk-ota-ble/`

Supported build helper:

```bash
tools/nrf/build_helix_ble_ota.sh v1
tools/nrf/build_helix_ble_ota.sh v2
```

Supported end-to-end smoke:

```bash
nix develop --command bash -lc 'tools/nrf/ble_ota_dk_smoke.sh /dev/ttyACM0 target/nrf52.cfg'
```

This flow:
- bootstraps the pinned NCS workspace if needed
- builds the `HelixOTA-v1` and `HelixOTA-v2` images
- recovers and flashes the DK with `v1`
- uploads the signed `v2` image over BLE from this PC
- waits for MCUboot promotion and the post-update `HelixOTA-v2` advertisement

For the locked supported procedure, use:
- [`docs/NRF_BLE_OTA_WORKFLOW.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF_BLE_OTA_WORKFLOW.md)
