# Claude Org Status

## Org Lead

- Lead session: claude-org team lead (Systems Architect + Review Board)
- Lead worktree: repo root
- Integration worktree: repo root

## Active Teams

| Team | Mission | Write Scope | Status |
|---|---|---|---|
| Systems Architect (lead) | Wave A acceptance guidance, threshold calibration, escalation criteria | `TASKS.md`, `.agents/`, `docs/` | idle — sprint 4 complete |
| Review Board (lead) | Ongoing signoff for Codex Wave A deliverables | Review-only | on-demand |
| Pose Inference | Experiment specs delivered (sprint 2) | `docs/` | idle |

## Sprint 4 Deliverables (2026-03-29)

| Deliverable | File |
|---|---|
| Wave A acceptance guide | `docs/WAVE_A_ACCEPTANCE_GUIDE.md` |
| Updated ORG_STATUS | This file |

## Key Findings (Sprint 4)

### Predicted Failure Points

- A1: ±90° pitch poses may show 3-5° instead of < 3° due to Mahony cross-product
  signal weakness near gimbal lock
- A3: Ki=0.0 WILL fail (by design — validates that Ki is needed)
- A4: Kp=0.5 from 90° yaw may not converge within 5s (expected Mahony behavior)
- A4: 180° initial error should NOT be tested (known Mahony antipodal issue)
- A5: 0.01 rad/s bias may overwhelm Ki=0.05 — start with 0.005 rad/s

### Intermediate Thresholds Published

All 6 Wave A tasks have three-tier thresholds: target / intermediate / floor.
Codex should use intermediate thresholds first, tighten to targets only when
the intermediate passes, and stop at the floor without faking success.

### Recommended Codex Execution Order

A5 → A1 → A3 → A2 → A4 → A6 (changed from original A1-first to A5-first
because Ki bias rejection is highest-value and fastest to validate).

## Planning Gate

- Problems currently owned: Wave A acceptance guidance, escalation criteria
- Writable scopes: `.agents/`, `docs/`, `TASKS.md`
- Review-only: all implementation code
- Blocked scopes: none
- No-duplication check: YES
- Approved to execute: YES

## Milestone Status

| Milestone | % | Notes |
|---|---|---|
| M1 | ~85% | Gaps: sensor validation matrix remainder (Wave B4) |
| M2 | ~50% | Harness hardened, scripted yaw done, Wave A in progress |
| M3 | ~20% | Cadence + transport + anchor tested |
| M4-M7 | 0% | Deferred |

## Signoff History

| Item | Verdict | Date |
|---|---|---|
| Codex Wave 1 (pre-23cd2ed) | Ready with conditions | 2026-03-29 |
| Codex harness hardening (23cd2ed) | APPROVED | 2026-03-29 |
| Codex scripted yaw + harness coverage | Pending formal review after Wave A | — |

## Next Activation Trigger

- When Codex completes A5 (Ki bias rejection) — quick review of actual drift values
- When Codex completes full Wave A — formal signoff cycle
- Or if Codex hits an escalation point (error > 45°, NaN, divergence)
