# Claude Org Status

## Org Lead

- Lead session: claude-org team lead (Systems Architect + Review Board)
- Lead worktree: .worktrees/claude-org
- Branch: claude/org

## Active Teams

| Team | Mission | Write Scope | Status |
|---|---|---|---|
| Systems Architect (lead) | Mainline redirect, escalation management | `TASKS.md`, `.agents/`, `docs/` | idle — sprint 5 complete |
| Review Board (lead) | Codex signoff | Review-only | on-demand |
| Pose Inference | Experiment specs (sprint 2) | `docs/` | idle |

## Sprint 5 Deliverables (2026-03-29)

| Deliverable | File |
|---|---|
| Mainline redirect note | `docs/SPRINT5_MAINLINE_REDIRECT.md` |

## Key Decision (Sprint 5)

A1 static ±90° yaw: 118° RMS. Mahony filter cannot converge from large
initial errors. This is a complementary filter linearization limitation.

- **A1 split:** A1a (small offsets ±15°, executable) + A1b (large offsets, blocked)
- **A4 rescoped:** 15° snap, not 90°
- **SensorFusion escalation filed:** MahonyAHRS needs initFromSensors()
- **Wave A reordered:** A3 → A1a → A2 → A6

## Revised Wave A Status

| Task | Status | Notes |
|---|---|---|
| A5 | Done | Approved with follow-ups |
| A3 | Next | 60s drift, identity start, Ki=0.02 |
| A1a | After A3 | ±15° offsets, intermediate thresholds |
| A2 | After A1a | Dynamic tracking from identity |
| A6 | After A2 | Two-node joint angle |
| A1b | BLOCKED | SensorFusion filter init |
| A4-large | BLOCKED | Same |

## Escalation

- **Problem:** MahonyAHRS::reset() → identity. No initFromSensors().
- **Fix:** TRIAD alignment from first accel+mag reading (~15 lines in SensorFusion)
- **Unblocks:** A1b, A4 large-angle convergence

## Signoff History

| Item | Verdict | Date |
|---|---|---|
| Codex Wave 1 | Ready with conditions | 2026-03-29 |
| Codex harness hardening (23cd2ed) | APPROVED | 2026-03-29 |
| A5 bias rejection (f9295c6) | Approved with follow-ups | 2026-03-29 |

## Milestone Status

| Milestone | % |
|---|---|
| M1 | ~85% |
| M2 | ~50% (narrowed scope achievable without filter init fix) |
| M3 | ~20% |
| M4-M7 | 0% |

## Next Activation

- Codex completes A3 — quick review
- SensorFusion filter init fix lands — unblock A1b/A4
