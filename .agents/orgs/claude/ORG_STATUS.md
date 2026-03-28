# Claude Org Status

## Org Lead

- Lead session: claude-org team lead (Systems Architect + Review Board)
- Lead worktree: .worktrees/claude-org
- Branch: claude/org

## Sprint 6 Deliverable

- `docs/SPRINT6_WAVE_A_RESCOPE.md`

## Key Decision (Sprint 6)

SensorFusion initFromSensors() has pitch/roll convention mismatch.
Yaw works. Pitch/roll broken. Commit yaw-only tests now, file bug, block
pitch/roll until fix lands.

## Wave A Status

| Task | Status |
|---|---|
| A5 Ki bias rejection | DONE |
| A3 60s drift | DONE |
| A6 joint angle recovery | DONE |
| A1a-yaw | READY — commit now |
| A2-yaw | READY — commit now |
| A1a-pitch/roll | BLOCKED — SensorFusion init convention |
| A2-pitch/roll | BLOCKED — same |
| A4 Kp/Ki sweep | DEFERRED — after init correct |
| A1b large-angle | DEFERRED — after init correct |

## Escalations

1. (Sprint 5) MahonyAHRS needs initFromSensors() → FIXED (214c28a)
2. (Sprint 6) initFromSensors() pitch/roll convention mismatch → NEW, filed

## Next Activation

- Codex commits yaw-only A1a/A2 — quick review
- SensorFusion pitch/roll fix lands — unblock remaining Wave A
