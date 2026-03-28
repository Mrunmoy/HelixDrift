# Claude Org Status

## Org Lead

- Lead session: claude-org team lead
- Lead worktree: repo root
- Integration worktree: repo root (Claude does not edit implementation code)

## Active Teams

| Team | Mission | Write Scope | Status |
|---|---|---|---|
| Systems Architect (lead) | Roadmap, backlog, interfaces, cross-org coordination | `TASKS.md`, `.agents/`, `docs/` (non-research) | active |
| Pose Inference (teammate) | Define v1 spatial output requirements and feasibility | `docs/pose-inference-*.md` | active |
| Review Board | Peer review of Codex/Kimi deliverables | Review-only across all scopes | on-demand |

## Planning Gate

- Problems currently owned:
  1. Pose inference v1 requirements definition — IN PROGRESS (teammate)
  2. Pose inference feasibility analysis — BLOCKED by #1
  3. Sensor validation acceptance criteria — COMPLETED
  4. SimulationHarness interface contract — COMPLETED
  5. Backlog reconciliation — COMPLETED

- Writable scopes currently claimed:
  - `.agents/`
  - `docs/` (excluding future `docs/research/`)
  - `TASKS.md`

- Review-only scopes:
  - `simulators/` (all code and tests)
  - `tests/`
  - `firmware/common/`
  - `tools/`
  - `examples/`
  - `.agents/orgs/codex/`
  - `.agents/orgs/kimi/`

- Blocked or contested scopes:
  - None.

- No-duplication check completed: YES
- Approved to execute: YES (operator gave go-ahead 2026-03-29)

## Completed Deliverables

| Deliverable | File | Date |
|---|---|---|
| Sensor validation matrix | `docs/sensor-validation-matrix.md` | 2026-03-29 |
| SimulationHarness interface spec | `docs/simulation-harness-interface.md` | 2026-03-29 |
| Unified execution plan | `docs/SIMULATION_TASKS.md` | 2026-03-29 |
| ORG_STATUS planning gate | `.agents/orgs/claude/ORG_STATUS.md` | 2026-03-29 |

## In Progress

| Task | Owner | Design Doc | Status |
|---|---|---|---|
| Pose inference requirements | pose-inference teammate | `docs/pose-inference-requirements.md` | in progress |
| Pose inference feasibility | pose-inference teammate | `docs/pose-inference-feasibility.md` | blocked by requirements |

## Reviews

- Requested from: none yet (will request after Codex starts executing)
- Received from: none yet
- Findings outstanding: none

## Integration State

- Ready to merge into claude integration: all completed docs above
- Waiting on fixes: none
- Ready for top-level integration: yes (docs only, no code conflicts)
