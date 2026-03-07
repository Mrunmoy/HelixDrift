# Tools

Repo-local toolchain and support assets for deterministic builds.

## Toolchains

- `tools/toolchains/arm-none-eabi-gcc.cmake`: cross toolchain file for Cortex-M4 targets.
- `tools/esp/setup_idf.sh`: bootstraps ESP-IDF toolchain for ESP32-S3.

## nRF Stubs

- `tools/nrf/stubs/include/`: minimal headers that allow off-target compile checks without full SDK.

These stubs are for build validation only, not for flashing real hardware.
