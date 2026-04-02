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

It is not yet the final HelixDrift BLE OTA implementation.

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

## Build A BLE Reference Sample

The current reference sample is Nordic's `peripheral_uart` on the `nrf52dk`.

```bash
tools/nrf/build_ncs_sample.sh
```

Defaults:
- sample: `nrf/samples/bluetooth/peripheral_uart`
- board: `nrf52dk/nrf52832`

Build output:
- `.deps/ncs/v3.2.4/build-nrf52dk-nrf52832-peripheral_uart/`

## Flashing

Once built, flash the resulting hex with the repo-local OpenOCD flow:

```bash
tools/nrf/flash_openocd.sh \
  .deps/ncs/v3.2.4/build-nrf52dk-nrf52832-peripheral_uart/merged.hex \
  target/nrf52.cfg
```

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

Still open:
- binding HelixDrift OTA transport to a real BLE peripheral implementation
- proving `v1 -> v2` over BLE on the DK
