# Ownership Matrix

This matrix is the default source of truth for which org should edit which
areas of the repository.

If a task needs exceptions, record them in the relevant org status doc before
editing begins.

## Codex

Primary owner:

- `simulators/i2c/`
- `simulators/sensors/`
- `simulators/gimbal/`
- `simulators/tests/`
- `tests/`
- `tools/`
- `examples/nrf52-mocap-node/`
- implementation work in `firmware/common/` when explicitly assigned

Review-only by default:

- `.agents/`
- most architecture docs under `docs/`

## Claude

Primary owner:

- `.agents/`
- `docs/`
- `TASKS.md`

Review-only by default:

- implementation directories
- simulator code
- platform code

May edit implementation only when:

- explicitly assigned an architecture-to-code interface cleanup task
- no other org is actively editing that scope

## Kimi

Primary owner:

- exploratory research docs under `docs/`
- `.agents/orgs/kimi/`
- future `docs/research/`

Review-only by default:

- implementation directories
- simulator code
- platform code

## Shared Areas

These are shared only through explicit ownership assignment:

- `firmware/common/`
- `README.md`
- validation docs under `docs/validation/`

Default shared-area rule:

- only one org edits a shared area at a time
- the owning org records the claim in its org status doc
- other orgs review only
