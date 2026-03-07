# HelixDrift

nRF mocap node firmware workspace (split from SensorFusion library-only repo).

## Quick Start

```bash
./build.py
```

## Build Commands

- `./build.py`: build all targets (host + nRF cross-build).
- `./build.py -t`: build all + run host tests.
- `./build.py --host-only -t`: host build and tests only.
- `./build.py --nrf-only`: nRF cross-build only.
- `./build.py --clean`: clean then build.

`build.py` auto-initializes `external/SensorFusion` for nRF builds.

## Repository Layout

- `firmware/common/`: shared embedded-safe logic (no heap/no RTTI/no exceptions).
- `examples/nrf52-blinky/`: baseline nRF blinky firmware app.
- `examples/nrf52-mocap-node/`: nRF mocap node firmware app using SensorFusion submodule.
- `tests/`: host-side TDD suite.
- `tools/`: repo-local toolchain and nRF stub headers for off-target validation.
- `datasheets/`: datasheets, reference manuals, and document index.

## Datasheets And Manuals

See:

- `datasheets/INDEX.md`
- `datasheets/README.md`

## Next Steps

- Wire node calibration command flow and timestamp sync with central node protocol.
- Add battery/health telemetry frames and CI gates for host + nRF smoke builds.

## nRF BLE Integration Boundary

- Implement `sf_mocap_ble_notify(const uint8_t* data, size_t len)` in board firmware.
- Optionally implement `sf_mocap_calibration_command(void)` returning:
  - `0`: none
  - `1`: capture stationary
  - `2`: capture T-pose
  - `3`: reset calibration
- Optionally implement `sf_mocap_sync_anchor(uint64_t* localUs, uint64_t* remoteUs)` to feed central-time sync anchors.
- Optionally implement `sf_mocap_health_sample(...)` to publish battery/link/drop telemetry; these are emitted as `NODE_HEALTH` frames.
- `examples/nrf52-mocap-node` uses `WeakSymbolBleSender` + `MocapNodeLoopT` to keep the loop testable off-target.
