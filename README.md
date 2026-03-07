# HelixDrift

nRF mocap node firmware workspace (split from SensorFusion library-only repo).

## Quick Start

```bash
nix develop --command bash -lc 'cmake -S . -B build-arm -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake && cmake --build build-arm -v'
```

## Next Steps

- Add `SensorFusion` as git submodule under `external/SensorFusion`
- Move nRF-specific mocap node code from SensorFusion into this repo
- Implement board BLE sender and application-level wiring
