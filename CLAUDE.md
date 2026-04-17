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
| **Hub** | Central receiver node (nRF52840 dongle). Collects data from all Tags over ESB, forwards to PC over USB CDC. |
| **Tag** | Body-worn mocap sensor node (nRF52840 ProPico). Streams orientation data to Hub over ESB. Each Tag has a unique FICR-based suffix (e.g. Tag-1-0D16). |

## Zephyr Apps

| App | Path | Role | Status |
|-----|------|------|--------|
| **Tag/Hub OTA** | `zephyr_apps/nrf52dk-ota-ble/` | BLE OTA firmware update for Tags (and Hub). Unique names, watchdog, negative-path tested. | **Active — production-ready for OTA** |
| **Tag/Hub Mocap Bridge** | `zephyr_apps/nrf52840-mocap-bridge/` | ESB mocap streaming. Build with `node.conf` for Tag, `central.conf` for Hub. | **Active — 2-node 100Hz proven** |
| **Tag ESB Link** | `zephyr_apps/nrf52840propico-esb-link/` | Earlier ESB smoke test app for split-host ProPico RF work. | Superseded by mocap bridge |

**Future:** Tag OTA + Tag Mocap Bridge will merge into a single Tag firmware with ESB/BLE mode switching. Same for Hub.

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
