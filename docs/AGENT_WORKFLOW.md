# Agent Workflow

This document defines the standard lifecycle for any agent-team change in
HelixDrift.

## Core Policy

Every feature follows this order:

1. Design first.
2. Planning gate and ownership check second.
3. TDD third.
4. Implementation and unit or integration testing fourth.
5. Fixes after test failures and review findings.
6. Documentation updates before PR and merge.
7. Merge only when CI passes.

Use `nix develop` for tooling, builds, and tests. Do not install project tools
globally on the machine.

## Feature Lifecycle

### 1. Design

Before implementation begins:

- write or update the relevant design note
- state assumptions, constraints, and interfaces
- identify ownership boundaries across agent teams

Design artifacts may live in:

- `docs/`
- `simulators/docs/`
- `.agents/`

### 2. TDD

Do not begin TDD or implementation until the planning gate has been satisfied
and ownership is clear across orgs.

Before implementation begins where practical:

- add or update tests that express the expected behavior
- keep tests narrow and attributable to the owning team
- separate single-sensor, fusion, sync, and platform failures where possible

### 3. Implementation

Implementation should:

- stay inside the assigned write scope
- minimize coupling across teams
- keep common logic host-testable
- keep platform code thin

### 4. Verification

At minimum, run the relevant local tests through `nix develop`.

Typical commands:

```bash
./build.py --clean --host-only -t
./build.py --nrf-only
```

If a narrower test command is used during development, run the broader
applicable suite before handoff.

Current CI split:

- host unit: `./build/host/helix_tests`
- host integration: `./build/host/helix_integration_tests`

There are not yet automated platform tests or on-target runtime tests in CI.

### 5. Documentation

Before PR or merge:

- update design docs
- update task tracking if scope changed
- update validation docs if test behavior changed
- update the development journal

### 6. Peer Review

Each feature requires at least 3 peer review rounds by other agent teams.

Suggested review pattern:

1. owning team self-review
2. adjacent technical team review
3. systems or integration review

Examples:

- Sensor work: reviewed by Fusion, Systems, and Host Tools
- RF/sync work: reviewed by Systems, Fusion, and nRF52
- Platform work: reviewed by Systems, RF/sync, and owning common-code team

### 7. CI And Merge

A feature is eligible for merge only when:

- local relevant tests pass
- documentation is updated
- peer review rounds are completed
- CI passes

## Branch Strategy

- use one branch per agent team or feature lane
- use one worktree per active agent team
- integrate through a central review branch or root worktree
- avoid overlapping file ownership during active development
- do not begin execution until the planning gate is complete

## Handoff Requirements

Every handoff should include:

- changed files
- design doc updates
- dev journal updates
- tests run
- known risks
- review rounds completed

Use `.agents/HANDOFF_TEMPLATE.md`.
