# CLAUDE.md

Branch-local guidance for `nrf-xiao-nrf52840`.

## Project

HelixDrift on this branch is an `nRF52` mocap-node codebase with a
simulation-first workflow. Host simulation is the source of truth for fusion,
sync, calibration, and multi-node behavior; the nRF path should stay thin and
follow the already-proven contracts.

## Nomenclature

| Term | Meaning |
|------|---------|
| **Hub** | Central receiver node (nRF52840 dongle). BLE name: `HHub-XXXX`. |
| **Tag** | Body-worn mocap sensor node (nRF52840 ProPico). BLE name: `HTag-XXXX`. |
| **XXXX** | 4-char hex suffix from nRF52840 FICR DEVICEADDR. Unique per chip, stable across reboots/OTA. |

## Product Architecture (agreed design)

```
Tag firmware (one binary):
  - ESB mocap streaming (default mode, always-on)
  - BLE OTA (on demand, triggered by Hub ESB command)
  - Radio is shared: ESB or BLE, never both at same time

Hub firmware (one binary):
  - ESB receive from all Tags
  - USB CDC forward to PC
  - NO BLE — Hub is always tethered to PC, updated over USB

OTA flow:
  1. PC sends firmware image to Hub over USB
  2. Hub sends "enter OTA" command to target Tag over ESB
  3. Tag stops ESB, starts BLE, advertises as HTag-XXXX
  4. Hub relays firmware to Tag over BLE
  5. Tag writes to slot 1, reboots
  6. MCUboot validates signed image, swaps A/B, boots new app
  7. Tag resumes ESB mocap streaming

MCUboot (bootloader):
  - Permanent at 0x0, never updated over OTA
  - Signs and validates images (ECDSA P-256)
  - A/B swap for rollback (swap-using-move, to be implemented)
  - NO BLE — stays minimal

Multi-user room isolation:
  - Each Hub derives a unique ESB pipe address from its FICR
  - Tags ship pre-configured with their Hub's pipe address
  - Tags on Hub A cannot hear Hub B — zero interference
```

**Design compromise:** BLE stack (~60KB) is baked into the Tag app binary even
though it only runs during OTA. Acceptable until firmware outgrows the 484KB
slot (~300KB headroom currently). If that happens, move BLE OTA into a custom
bootloader extension.

## Zephyr Apps (current state)

| App | Path | Role | Status |
|-----|------|------|--------|
| **Tag/Hub OTA** | `zephyr_apps/nrf52dk-ota-ble/` | Standalone BLE OTA app. Will be merged into Tag/Hub firmware. | **Proven — to be merged** |
| **Tag/Hub Mocap Bridge** | `zephyr_apps/nrf52840-mocap-bridge/` | ESB mocap streaming. `node.conf` for Tag, `central.conf` for Hub. | **Active — 2-Tag 100Hz proven** |
| **Tag ESB Link** | `zephyr_apps/nrf52840propico-esb-link/` | Earlier ESB smoke test. | Superseded |

**Next step:** Merge OTA modules (`firmware/common/ota/`) into the mocap bridge
app so each Tag has one firmware that does both ESB and BLE OTA.

## Build Commands

```bash
./build.py --host-only -t
./build.py --nrf-only
./build.py --clean
./magic.sh
```

## Priorities

- preserve host-test coverage
- keep common code platform-agnostic
- keep the nRF platform path healthy
- do not reintroduce secondary-MCU-specific paths on this branch

## Workflow

1. Write or update tests first where practical.
2. Implement in `firmware/common/`, `simulators/`, or the nRF example as
   appropriate.
3. Run `./build.py --host-only -t`.
4. If platform code changed, run `./build.py --nrf-only`.
5. Update docs and task tracking before handoff.
