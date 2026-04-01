# HelixDrift AGENTS.md

AI coding agent guidance for the `nrf-xiao-nrf52840` branch.

## Project Overview

HelixDrift is a simulation-first mocap node firmware workspace centered on the
`nRF52` integration path. The branch proves fusion, sync, calibration, and
multi-node behavior on host first, then carries the validated runtime onto
`nRF52840` hardware.

Current direction:
- primary MCU target: `nRF52840`
- primary validation path: host simulation and host tests
- next platform step: nRF bring-up using the same contracts proven in
  simulation

Target sensor stack:
- IMU: `LSM6DSO`
- Magnetometer: `BMM350`
- Barometer: `LPS22DF`

## Build Commands

```bash
./build.py --host-only -t
./build.py --nrf-only
./build.py --clean
./magic.sh
```

## Repository Layout

- `firmware/common/`: platform-agnostic embedded-safe runtime code
- `examples/nrf52-mocap-node/`: main nRF52840 platform path
- `examples/nrf52-blinky/`: minimal nRF smoke example
- `simulators/`: host simulation and integration harnesses
- `tests/`: host unit tests
- `tools/`: repo-local build helpers and toolchains
- `external/SensorFusion/`: fusion, codec, and driver submodule

## Branch Rules

- Treat this branch as nRF-only.
- Do not add secondary-MCU-specific code or docs here.
- If a platform-specific fix belongs to SensorFusion, fix it there first, then
  update the submodule pointer here.

## Workflow

- Test-first where practical.
- Keep common logic host-testable.
- Run `./build.py --host-only -t` before handoff.
- Run `./build.py --nrf-only` when the nRF path changes.
- Update `TASKS.md`, `simulators/docs/DEV_JOURNAL.md`, and
  `.agents/orgs/codex/ORG_STATUS.md` when milestone state changes.

## Hardware Debug Note

- For visible board-heartbeat or blink-code checks, prefer the external
  `hardware-camera-mcp` tool if it is registered in the client.
- Expected location: `~/sandbox/mcp/hardware-camera-mcp`
- Useful tools from that MCP:
  - `capture_frame`
  - `capture_sequence`
  - `find_dynamic_region`
  - `measure_led_blink_rate`
- Use it when serial logs are absent or when firmware timing needs to be
  compared against a visible LED heartbeat on real hardware.
