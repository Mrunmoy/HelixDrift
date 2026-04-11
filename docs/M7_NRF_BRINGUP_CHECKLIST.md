# M7 nRF Bring-Up Checklist

This checklist is the first hardware phase after the M1-M6 simulation work on
the `nrf-xiao-nrf52840` branch.

It assumes:
- the runtime contracts were proven in host simulation
- OTA uses the dual-slot plan in
  [`docs/NRF_OTA_MEMORY_PLAN.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF_OTA_MEMORY_PLAN.md)
- tooling is run through `nix develop`
- the BLE reference workflow is defined in
  [`docs/NRF_BLE_REFERENCE_WORKFLOW.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF_BLE_REFERENCE_WORKFLOW.md)

## Current Bring-Up Reality

As of this branch state:

- a physical Nordic nRF52 DK is available and reachable over its onboard
  SEGGER J-Link probe
- OpenOCD can halt and program the attached target successfully
- the connected board identifies as `nRF52832`, not `nRF52840`
- a dedicated bare-metal DK self-test image now boots on hardware, updates a
  retained status block in RAM, drives the board LEDs directly, and proves
  real internal flash erase/write/verify on the target
- a repo-local Nordic/Zephyr BLE reference lane can now be bootstrapped under
  `.deps/ncs/` and built from `nix develop` without depending on a personal
  NCS install

Official nRF52 DK LED and button mapping from the Nordic user guide:

- Button 1: `P0.13`
- Button 2: `P0.14`
- Button 3: `P0.15`
- Button 4: `P0.16`
- LED 1: `P0.17`
- LED 2: `P0.18`
- LED 3: `P0.19`
- LED 4: `P0.20`

The DK LEDs are active low.

So this board should be treated as:
- valid for generic nRF bring-up, flashing, runtime, and OTA-path work
- not a final proof of the 52840-specific flash budget or final hardware layout

## Goals

1. Prove the nRF application boots on real hardware.
2. Prove the basic sensor board path works with substitute sensors.
3. Prove the OTA flash backend actually writes real flash.
4. Prove the BLE OTA transport works end to end.
5. Use the two plugged-in nRF dongles later for simple RF sanity checks.

## Available Devices Detected On This Machine

Detected during this session:

- `/dev/ttyACM0`
  - stable path:
    `/dev/serial/by-id/usb-ZEPHYR_USB-DEV_6EA803F6F395E7C6-if00`
- `/dev/ttyACM1`
  - stable path:
    `/dev/serial/by-id/usb-ZEPHYR_USB-DEV_481AC747AA2250F5-if00`

Both devices:
- enumerate as `NordicSemiconductor USB-DEV`
- are accessible from the current user
- can be opened directly from this shell

Use the `/dev/serial/by-id/...` names instead of raw `ttyACM*` names when
possible.

## Tooling Rule

Run bring-up commands through:

```bash
nix develop
```

The current shell already has the cross-compiler available there. Additional
nRF flashing/debug tools may need to be added to the flake later if they are
not already included.

Current note:
- the Zephyr/Nordic host-side build dependencies needed for BLE reference work
  are now included in `nix develop`
- the current `nRF52840` dongle getting-started flow is documented in
  [`docs/NRF52840_DONGLE_GETTING_STARTED.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF52840_DONGLE_GETTING_STARTED.md)
- for J-Link-based bring-up, use the repo-local sequential build+flash helper
  to avoid artifact races:

```bash
tools/nrf/build_and_flash_jlink.sh nrf52dk_bringup 1050335103 NRF52832_XXAA 2
tools/nrf/build_and_flash_jlink.sh nrf52840propico_bringup 123456 NRF52840_XXAA 0
```

- for split-host multi-board development, keep this checkout as the source of
  truth and mirror it to the second machine before remote flash:

```bash
tools/dev/sync_remote_workspace.sh litu@hpserver1 /home/litu/sandbox/embedded/HelixDrift
tools/nrf/remote_build_and_flash.sh litu@hpserver1 nrf52840propico_bringup 123456 NRF52840_XXAA 0 /home/litu/sandbox/embedded/HelixDrift
```

Split-host note:
- this is the current workaround for duplicate-serial clone `J-Link-OB` probes
- each machine only sees one probe, so probe identity collisions disappear
- the remote mirror is not an authoritative checkout; resync it before remote
  build/flash

Reset note:
- `RSetType 0` (`NORMAL`) may use `SYSRESETREQ`, not a hardware reset pin
- `RSetType 2` (`RESETPIN`) should be preferred when the probe and board wiring
  support it and cold-boot-equivalent behavior matters
- bare-metal bring-up timing should not rely on `DWT_CYCCNT`; use SysTick-based
  delays instead

## Phase 1: Boot And Binary Sanity

### Goal

Prove the built image is structurally correct before debugging sensor or BLE
behavior.

### Checklist

- [x] Build the nRF target in `nix develop`
- [x] Inspect the produced ELF, BIN, or HEX artifacts
- [x] Confirm the DK self-test target links at `0x00000000` for bare-metal
      bring-up on the available board
- [x] Confirm no unexpected section growth has occurred

### Commands

```bash
nix develop --command bash -lc './build.py --nrf-only'
```

### Pass Condition

- clean nRF build
- expected output artifacts exist
- app image size remains comfortably below the slot budget

Current useful board-specific artifacts:

- `build/nrf/nrf52dk_blinky.hex`
- `build/nrf/nrf52dk_bringup.hex`
- `build/nrf/nrf52dk_selftest.hex`

## Phase 1.5: BLE Reference Lane

### Goal

Prove that this repository can drive a BLE-capable Nordic reference build from
`nix develop` without relying on a separately installed personal NCS toolchain.

### Checklist

- [x] provide Zephyr/Nordic host utilities through `nix develop`
- [x] bootstrap a pinned Nordic workspace under `.deps/ncs/`
- [x] build a BLE reference sample for `nrf52dk/nrf52832`
- [x] recover, flash, and observe the reference BLE sample on hardware

### Commands

```bash
nix develop --command bash -lc 'tools/dev/doctor.sh'
nix develop --command bash -lc 'tools/nrf/build_ncs_sample.sh'
```

### Pass Condition

- the BLE reference workspace is bootstrapped locally under `.deps/`
- the reference sample builds from the nix shell
- no separately installed personal NCS toolchain is required
- the PC can discover the DK over BLE and deliver a payload that is visible on
  the DK UART console

## Phase 2: Board Bring-Up

### Goal

Prove the real nRF board boots, reaches main, and does not immediately fault.

### Checklist

- [x] Flash a real DK-targeted image using the agreed nRF tool
- [x] Open serial log on the board port
- [x] Confirm startup reaches application main loop
- [x] Confirm no immediate reset loop or hard fault
- [x] Confirm watchdog is not tripping immediately

### First Signals To Add Or Verify

- boot banner
- build/version string
- boot count
- startup result for sensor init
- OTA confirmation log point

### Pass Condition

- stable boot
- readable serial output, or another explicit runtime observability path
- no reset loop

Current note:
- board-correct flashing is already proven via OpenOCD
- runtime observability is now proven through a retained `.noinit` status block
  read back over SWD:
  - status base: `0x20000018`
  - observed pass state:
    - magic `0x48445837`
    - phase `6` (`Passed`)
    - heartbeat `0x0000001d`
    - LED sweep count `3`
    - flash verified words `4`
    - failure code `0`
- the self-test also leaves a success signature in flash at `0x0007F000`:
  - `0x48444B31`
  - `0x4F4B4159`
  - `0x00000004`
  - `0x0007F000`
- serial/VCOM output from the custom bring-up app is now proven on
  `/dev/ttyACM0` using the DK's documented `P0.06/P0.08` UART routing
- repo-local helpers exist for this path:
- `tools/nrf/read_symbol_words.sh`
- `tools/nrf/read_nrf52dk_selftest.sh`

## Phase 5: BLE OTA Validation

### Goal

Prove that the real Helix OTA control/data path runs over BLE on the DK, not
just through synthetic traffic or UART/VCOM.

### Checklist

- [x] Build a repo-local Helix BLE OTA target app from `nix develop`
- [x] Flash `HelixOTA-v1` to the DK
- [x] Discover the target over BLE from this PC
- [x] Upload a signed `v2` image over BLE
- [x] Stage and commit the image through the real Helix OTA backend
- [x] Observe MCUboot promotion and `HelixOTA-v2` after reboot
- [x] Reject a wrong-target OTA before `BEGIN` is accepted

### Commands

```bash
nix develop --command bash -lc 'tools/nrf/ble_ota_dk_smoke.sh /dev/ttyACM0 target/nrf52.cfg'
```

### Pass Condition

- the DK advertises `HelixOTA-v1`
- the PC uploads the signed `v2` image over BLE
- the board reboots through MCUboot into `v2`
- the DK advertises `HelixOTA-v2` after the update

### Failure Handling Follow-Up

- [x] bad CRC transfer is rejected and does not promote `v2`
- [x] wrong target ID is rejected immediately and the DK remains on `v1`
- [x] explicit abort returns the target to idle on `v1`
- [x] a later valid BLE OTA still succeeds after those failures

Current negative smoke:

```bash
nix develop --command bash -lc 'tools/nrf/ble_ota_negative_smoke.sh /dev/ttyACM0 target/nrf52.cfg'
```

## Phase 5.5: nRF52840 Dongle OTA Lane

### Goal

Carry the OTA path from the proven `nRF52 DK` reference lane onto the intended
`nRF52840` target lane.

### Current Status

- [x] `nrf52840dongle/nrf52840/bare` build path exists
- [x] first flash to the dongle works through J-Link
- [x] the dongle boots and advertises `Helix840-v1`
- [ ] clean `Helix840-v1 -> Helix840-v2` BLE OTA proof on the first dongle
- [ ] same BLE OTA proof on the second 52840 target
- [ ] final 52840 README/how-to lock-in after those two proofs pass

### Current References

- getting started:
  [`docs/NRF52840_DONGLE_GETTING_STARTED.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF52840_DONGLE_GETTING_STARTED.md)
- exact blocker checkpoint:
  [`docs/NRF52840_BLE_OTA_CHECKPOINT.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF52840_BLE_OTA_CHECKPOINT.md)

## Phase 3: Sensor Bring-Up With Substitute Sensors

### Goal

Prove the firmware can talk to real attached sensors, even if they are not the
final production parts.

### Checklist

- [ ] verify I2C buses come up
- [ ] probe each attached sensor and log device ID / WHOAMI
- [ ] read basic accel/gyro/mag/baro samples
- [ ] log sample cadence and obvious failure states
- [ ] confirm the platform tolerates missing optional sensors cleanly

### Notes

The substitute-sensor phase is for:
- bus validation
- timing validation
- real noise / startup behavior
- firmware integration smoke checks

It is not for:
- final calibration constants
- exact production accuracy claims

### Pass Condition

- firmware detects the real devices present
- sample reads succeed consistently
- basic data looks live rather than stuck or zeroed

## Phase 4: OTA Flash Backend Validation

### Goal

Prove [`NrfOtaFlashBackend.cpp`](/home/mrumoy/sandbox/embedded/HelixDrift/examples/nrf52-mocap-node/src/NrfOtaFlashBackend.cpp)
works on real flash rather than only in host tests.

### Checklist

- [x] prove real erase on target flash
- [x] write a known test pattern
- [x] read back and compare
- [x] repeat with unaligned and tail-partial writes through the OTA backend
- [x] verify full-slot bounds enforcement through the OTA backend
- [x] prove a staged secondary image can be marked pending and booted through
      MCUboot on the DK

### Critical Questions

- do real NVMC writes complete correctly?
- are alignment rules handled correctly on target?
- does erase timing behave acceptably?
- does the actual board require any toolchain or stub separation fix?

### Pass Condition

- data written to target flash matches what was sent
- no silent no-op flash writes
- no corruption on unaligned writes once the OTA-backend path is exercised

Current note:
- the DK self-test has already proven the low-level NVMC erase/write/verify
  path on real hardware
- the updated DK self-test now also proves the repo's OTA-backend word-packing
  path on real hardware, including:
  - sequential chunk writes that cross a word boundary
  - tail-partial writes at the end of the test slot
  - overflow rejection beyond the configured slot size
- the updated DK self-test also proves the OTA control/state path on hardware:
  - `BleOtaService` begin/data/commit command parsing
  - `OtaManager` state transitions and CRC validation
  - committed-state readback after a synthetic image transfer into target flash
- the standalone DK OTA smoke now also proves the MCUboot promotion step:
  - mass erase board
  - flash `nrf52dk_bootloader`
  - flash signed `nrf52dk_ota_probe_v1`
  - stage signed `nrf52dk_ota_probe_v2` into slot 1
  - write overwrite-only permanent pending trailer
  - reboot into `v2` and observe live serial output on `/dev/ttyACM0`
- the repo now also proves a real transport-driven remote update on the DK:
  - flash signed `nrf52dk_ota_serial_v1`
  - upload signed `nrf52dk_ota_serial_v2` over `/dev/ttyACM0`
  - commit via the real backend/service path
  - reboot and observe `ota-v2` over the same UART path
- the remaining OTA transport work is the real BLE path into the already-proven
  backend and bootloader flow

## Phase 5: BLE OTA Service Validation

### Goal

Prove the app can receive a firmware update into the secondary slot and request
upgrade.

### Checklist

- [ ] expose OTA GATT service
- [ ] send begin command with image size + CRC
- [ ] stream image chunks
- [ ] observe status updates
- [ ] commit the image
- [ ] reboot into updated image
- [ ] confirm new image only after healthy startup

### Pass Condition

- complete transfer succeeds
- new image boots
- new image can be confirmed or retained according to the chosen OTA policy
- confirmation logic works

Current note:
- The OTA transport semantics are now proven over UART/VCOM on the DK.
- BLE transport is still open, but it is now a transport-binding problem rather
  than a storage/commit/MCUboot reliability problem.

## Phase 6: Failure Cases

### Goal

Prove the update flow fails safely.

### Checklist

- [ ] abort mid-transfer
- [ ] send wrong CRC
- [ ] send incomplete image
- [ ] reboot before commit
- [ ] verify previous working image remains usable

### Pass Condition

- bad transfer does not brick the device
- old working image remains recoverable

## Phase 7: nRF Dongle RF Sanity Checks

Use the two detected dongles only after the basic board bring-up and OTA path
are healthy.

### Suggested Roles

- dongle A: anchor / master
- dongle B: node / receiver

### First RF Experiments

- [x] packet transmit / receive smoke test on two split-host `nRF52840`
      ProPicos using the repo-local Zephyr ESB link app
- [ ] repeated anchor broadcast at fixed cadence
- [ ] measure drop rate in a normal room
- [ ] compare real latency/jitter behavior against M4 simulation assumptions

Useful command:

```bash
nix develop --command bash -lc \
  'tools/nrf/propico_esb_split_host_smoke.sh \
     litu@hpserver1 /home/litu/sandbox/embedded/HelixDrift \
     123456 123456 NRF52840_XXAA 3000'
```

### Use This For

- cheap real RF timing probes
- first anchor timing validation
- packet-loss sanity checks

### Do Not Use This For

- final wearable-node proof
- final product latency claims
- final sensor-fusion validation

## Recommended Execution Order

1. Boot and binary sanity
2. Board bring-up
3. Sensor bring-up
4. OTA flash backend validation
5. BLE OTA validation
6. Failure-case validation
7. Dongle RF sanity checks

## Stop Conditions

Stop and fix fundamentals before proceeding if any of these occur:

- board will not boot stably
- serial output is absent or resets loop
- flash writes are no-ops on target
- OTA receive path corrupts data
- new image cannot boot reliably

Do not start RF experiments before OTA flash and basic board behavior are
credible.
