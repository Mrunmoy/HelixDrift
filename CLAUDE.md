# CLAUDE.md

Branch-local guidance for `nrf-xiao-nrf52840`.

## Project

HelixDrift on this branch is an `nRF52` mocap-node codebase with a
simulation-first workflow. Host simulation is the source of truth for fusion,
sync, calibration, and multi-node behavior; the nRF path should stay thin and
follow the already-proven contracts.

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
