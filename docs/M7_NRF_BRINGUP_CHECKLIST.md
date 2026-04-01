# M7 nRF Bring-Up Checklist

This checklist is the first hardware phase after the M1-M6 simulation work on
the `nrf-xiao-nrf52840` branch.

It assumes:
- the runtime contracts were proven in host simulation
- OTA uses the dual-slot plan in
  [`docs/NRF_OTA_MEMORY_PLAN.md`](/home/mrumoy/sandbox/embedded/HelixDrift/docs/NRF_OTA_MEMORY_PLAN.md)
- tooling is run through `nix develop`

## Current Bring-Up Reality

As of this branch state:

- a physical Nordic nRF52 DK is available and reachable over its onboard
  SEGGER J-Link probe
- OpenOCD can halt and program the attached target successfully
- the connected board identifies as `nRF52832`, not `nRF52840`
- a dedicated bare-metal DK self-test image now boots on hardware, updates a
  retained status block in RAM, drives the board LEDs directly, and proves
  real internal flash erase/write/verify on the target

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

## Phase 2: Board Bring-Up

### Goal

Prove the real nRF board boots, reaches main, and does not immediately fault.

### Checklist

- [x] Flash a real DK-targeted image using the agreed nRF tool
- [ ] Open serial log on the board port
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
- serial/VCOM output from the custom bring-up app is still open, but it is no
  longer the only runtime observability path

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
- [ ] repeat with unaligned and tail-partial writes through the OTA backend
- [ ] verify full-slot bounds enforcement through the OTA backend

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
- the remaining work is to prove the repo's OTA-backend code paths, not whether
  the target flash can be programmed at all

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
- confirmation logic works

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

- [ ] packet transmit / receive smoke test
- [ ] repeated anchor broadcast at fixed cadence
- [ ] measure drop rate in a normal room
- [ ] compare real latency/jitter behavior against M4 simulation assumptions

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
