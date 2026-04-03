# nRF52840 BLE OTA Checkpoint

Date: 2026-04-03

This note captures the exact state of the `nrf52840` BLE OTA lane at the end
of the current session.

## Proven So Far

- `nrf52840dongle/nrf52840/bare` builds cleanly from the repo-local NCS
  workspace under `nix develop`.
- MCUboot on the `nrf52840` dongle now fits cleanly after trimming the default
  dongle bootloader feature set.
- The first `nRF52840` dongle boots the repo-local `Helix840-v1` image and
  advertises over BLE.
- The uploader-side `BEGIN` timeout issue is understood:
  the 52840 full-slot erase takes longer than the old default GATT timeout.
- With the longer `BEGIN` timeout, OTA no longer fails immediately at startup.
- The `nrf52840` OTA lane is using a distinct target ID:
  `0x52840059`.

## Changes Made In This Session

- Extended the BLE uploader timeouts and recovery behavior so the 52840 path
  can survive the longer slot erase and tolerate reconnects.
- Corrected stale helper assumptions around the `nrf52840` OTA footer and
  partition expectations.
- Updated the target-side Zephyr OTA backend to stop using the old ad-hoc
  28-byte footer assumption and instead align with the current MCUboot footer
  layout for this build.
- Confirmed from the generated app config that the 52840 lane currently reports:
  - `CONFIG_MCUBOOT_BOOTLOADER_MODE_OVERWRITE_ONLY=1`
  - `CONFIG_MCUBOOT_UPDATE_FOOTER_SIZE=0x30`

## What Is Still Not Proven

- A clean, repeatable end-to-end `Helix840-v1 -> Helix840-v2` BLE OTA success
  on the first `nRF52840` dongle.
- The same end-to-end BLE OTA success on the second `nRF52840` target.
- Final README/how-to lock-in for the 52840 BLE OTA workflow.

## Remaining Blocker

The unresolved issue is no longer generic BLE bring-up.

The remaining blocker is the final `slot1 -> pending -> reboot into v2`
closure on `nrf52840dongle/nrf52840/bare`.

In other words:

- advertising works
- `BEGIN` works with the longer timeout
- transfer progresses on the real target
- but the final `Helix840-v2` promotion proof is still open

## Next Steps

1. Re-run the first-dongle `Helix840-v1 -> Helix840-v2` BLE OTA proof with the
   corrected footer logic already patched into the target backend.
2. If the first dongle passes, repeat the same proof on the second 52840
   target.
3. Only after both pass should the README/how-to docs be updated to lock the
   52840 BLE OTA lane for day-to-day use.
