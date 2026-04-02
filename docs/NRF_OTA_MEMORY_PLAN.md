# nRF OTA Memory Plan

This document defines the recommended flash layout and firmware update flow
for the `nrf-xiao-nrf52840` branch.

## Goals

- Keep OTA updates recoverable during development.
- Preserve a known-good image while a new image is transferred.
- Avoid premature flash-layout optimization before real BLE and board-level
  integration are measured.
- Keep the branch aligned with the current MCUboot-style boot flow already
  encoded in the linker scripts and OTA backend.

## Current Branch Assumptions

The current linker layout in
[`tools/linker/xiao_nrf52840_app.ld`](/home/mrumoy/sandbox/embedded/HelixDrift/tools/linker/xiao_nrf52840_app.ld)
is:

| Region | Start | Size |
|---|---:|---:|
| Bootloader | `0x00000000` | `96 KB` |
| Primary slot | `0x00018000` | `352 KB` |
| Secondary slot | `0x00070000` | `352 KB` |
| Scratch | `0x000C8000` | `32 KB` |
| NVS / calibration | `0x000D0000` | `192 KB` |

The current application image is small relative to that layout.

Latest measured nRF app size:
- `text`: `27152 B`
- `data`: `96 B`
- `bss`: `8560 B`
- total reported by `arm-none-eabi-size`: `35808 B`

That means the current app occupies under `10%` of a `352 KB` slot.

## Recommendation

## 1. Use Two Application Slots

Recommended: `yes`

Do not move to a one-slot OTA layout on this branch.

Reason:
- BLE OTA writes are interruptible and failure-prone compared to wired update.
- The safest default is to keep the running image intact while writing the new
  image elsewhere.
- A second slot gives recovery room while the firmware and BLE stack are still
  evolving.

Practical conclusion:
- Keep `primary + secondary`.
- Continue writing OTA payloads into the secondary slot.

## 2. Keep the Current Slot Sizes for Now

Recommended current layout:
- Bootloader: `96 KB`
- Primary slot: `352 KB`
- Secondary slot: `352 KB`
- Scratch: `32 KB`
- NVS/calibration: `160 KB`

Reason:
- The app is currently much smaller than the slot.
- Real size growth has not happened yet:
  - real BLE stack integration
  - real sensor-board code
  - production calibration persistence
  - logging / diagnostics
- Shrinking slots now would optimize the wrong constraint too early.

## 3. Development OTA vs Product OTA

Treat these as separate phases.

### Development OTA

Recommended now:
- Dual-slot update flow
- Conservative slot sizes
- Simpler policy over maximum density

This is the right choice while:
- board bring-up is still ahead
- OTA is still being validated
- image growth is not yet characterized on target

### Product OTA

Re-evaluate later after on-device BLE OTA and full board integration.

At that point decide whether to keep:
- overwrite-only with confirmation discipline, or
- swap/revert-capable flow for stronger failed-update recovery

That decision should be based on:
- real image size
- real update duration
- real flash erase/write timing
- acceptable field-failure risk

## Update Flow

Recommended firmware update flow:

1. Device boots current primary image through MCUboot.
2. Running app exposes BLE OTA service.
3. OTA client sends:
   - total image size
   - expected CRC32
4. App erases the secondary slot.
5. App writes sequential chunks into secondary slot.
6. App verifies transfer integrity in software via CRC32.
7. App marks the image as pending upgrade.
8. Device reboots.
9. Bootloader validates the image and boots the new version.
10. New app confirms itself only after healthy startup.

Healthy startup should include, at minimum:
- boot path reached application main loop
- required sensors initialized or were deliberately declared optional
- persistent configuration loaded
- no immediate watchdog or fatal startup condition

## Confirmation Policy

Confirmation should not happen immediately on boot.

Recommended:
- boot into the new image unconfirmed
- run minimal health checks
- only then confirm the image

Why:
- if the image is broken but boots briefly, early confirmation removes your
  rollback safety
- startup confirmation should mean "device is functionally alive", not merely
  "reset vector worked"

## One Slot vs Two Slots

### One Slot

Do not use this now.

Pros:
- more flash available for app or storage

Cons:
- failed update can destroy the only runnable image
- more fragile update logic
- poorer recovery story during development
- bad tradeoff while real hardware behavior is still unknown

### Two Slots

Use this now.

Pros:
- preserves known-good image during transfer
- simpler operational model
- aligns with current linker/backend design
- gives safer bring-up and field testing

Cons:
- consumes more internal flash

That flash cost is acceptable at the current app size.

## Scratch Region

Current scratch allocation: `32 KB`

Keep it for now.

Even if the present flow is effectively overwrite-oriented, the reserved
scratch region is cheap insurance while the boot/update policy is still being
validated. Do not reclaim it until the final boot strategy is fixed and proven
on hardware.

## Space Budget Guidance

Use these thresholds as practical decision gates.

### Safe

- App image under `200 KB`
- No layout pressure

### Watch

- App image `200-280 KB`
- Start measuring growth per feature
- Avoid careless debug/log bloat

### Re-plan

- App image above `280-300 KB`
- Revisit:
  - slot sizing
  - NVS reservation
  - boot mode
  - whether secondary slot still has enough headroom for signed/padded images

## What to Validate on Real Hardware

Before freezing the OTA layout, validate:

1. Bootloader can boot the application reliably.
2. OTA writes really persist to internal flash.
3. CRC32 catches corrupted transfers.
4. Pending upgrade path works end to end.
5. Reboot into new image works.
6. Confirmation path works only after healthy startup.
7. Interrupted transfer leaves current app usable.
8. Full-size near-slot-limit image behavior is understood.

## Decision Summary

- Two app slots: `yes`
- One app slot: `no`
- Keep current slot sizes: `yes`
- Revisit after first real nRF BLE OTA measurements: `yes`
- Freeze final product OTA layout now: `no`

## Immediate Next Step

Use the current dual-slot layout during M7 bring-up.

Do not optimize flash layout first. First prove:
- real board boots
- real flash backend writes correctly
- real BLE OTA path functions
- real image size remains comfortably inside the slot budget
