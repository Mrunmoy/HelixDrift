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

## Repository Layout

- `firmware/common/`: shared embedded-safe logic (no heap/no RTTI/no exceptions).
- `examples/nrf52-blinky/`: baseline nRF blinky firmware app.
- `tests/`: host-side TDD suite.
- `tools/`: repo-local toolchain and nRF stub headers for off-target validation.
- `datasheets/`: datasheets, reference manuals, and document index.

## Datasheets And Manuals

See:

- `datasheets/INDEX.md`
- `datasheets/README.md`

## Next Steps

- Add `SensorFusion` as git submodule under `external/SensorFusion`.
- Move nRF-specific mocap node code from SensorFusion into this repo.
- Implement board BLE sender and application-level wiring.
