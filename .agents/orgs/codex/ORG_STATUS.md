# Codex Org Status

## Org Lead

- Lead session: Codex main session
- Lead worktree: `.worktrees/codex-active`
- Integration worktree: `.worktrees/codex-active`

## Active Teams

| Team | Worktree | Mission | Write Scope | Status |
|---|---|---|---|---|
| Sensors | `.worktrees/codex-active` | Prove each sensor independently and improve simulator fidelity | `simulators/i2c/`, `simulators/sensors/`, `simulators/tests/` | active |
| Fusion | `.worktrees/codex-active` | Build host-side virtual sensor and node harnesses on top of the proven simulator stack | `simulators/fixtures/`, `simulators/tests/`, `tests/` | active |
| Host Tools |  | Defer until sensor validation needs evidence tooling changes | review-only for now | idle |
| nRF52 |  | Defer until simulation proof work advances | `examples/nrf52-mocap-node/`, `tools/nrf/` | idle |

## Planning Gate

- Problems currently owned:
  - Per-sensor validation matrix and standalone proof criteria
  - Deterministic simulator behavior needed for quantitative sensor tests
  - Register-behavior and scale-behavior proof items that are already
    implemented in the simulators
  - Host-side virtual sensor-assembly harness for reusable dual-bus,
    three-sensor composition
  - Host-side virtual mocap node harness with cadence, capture transport, and
    timestamp-mapping proof
  - Basic pose-quality metrics suitable for bounded host assertions
  - Scripted yaw motion-regression assertions on the virtual node harness
  - Batched run support with summary pose-error stats for future experiments
  - Harness safety and deterministic-seeding coverage in the Codex worktree
  - Wave A A5 Mahony bias-rejection proof in a standalone experiment file
  - Wave A A1 static-yaw probe and escalation evidence
- Writable scopes currently claimed:
  - `simulators/sensors/`
  - `simulators/fixtures/`
  - `simulators/tests/`
  - `docs/`
  - `simulators/docs/DEV_JOURNAL.md`
- Review-only scopes:
  - `.agents/`
  - `TASKS.md`
  - RF/sync design topics owned by Kimi
  - Pose requirements and systems planning owned by Claude
- Blocked or contested scopes:
  - `firmware/common/` unless needed by a later assigned Codex task
  - any scope actively claimed later by another org
- No-duplication check completed:
  - Codex owns sensor implementation and sensor tests
  - Codex does not own primary RF/sync research
  - Codex does not own primary pose-inference requirements work
- Approved to execute:
  - Yes, for the Sensor Validation lane only

## Claimed Scopes

- Active implementation ownership:
  - `simulators/sensors/`
  - `simulators/tests/`
- Review-only areas:
  - `.agents/`
  - most architecture docs under `docs/`
- Conflicts or blocked scopes:
  - none active at this time

## Current Work

- Task: Add deterministic per-sensor simulator seeding and document standalone sensor proof criteria
- Task: Deliver the Sensor Validation slice, reusable sensor-assembly harness,
  virtual mocap node harness, and first pose-metric assertions for host
  integration work
- Task: Close M2 Wave A incrementally from the Codex worktree, starting with
  harness-safety coverage and scripted yaw regressions before broader pose
  experiments
- Task: Follow Claude Wave A sequencing, but escalate any task that fails its
  intermediate-entry conditions instead of forcing false acceptance tests
- Design doc: `docs/PER_SENSOR_VALIDATION_MATRIX.md`
- Tests first: yes
- Journal updated: yes (`simulators/docs/DEV_JOURNAL.md`)

## Reviews

- Requested from:
- Received from:
- Findings outstanding:

## Integration State

- Ready to merge into codex integration: yes
- Waiting on fixes: no
- Ready for top-level integration: yes, once the shared tree is clean enough to
  merge `codex/active`
