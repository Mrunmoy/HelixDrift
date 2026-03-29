# Agent Bootstrap Packet

Use this file to onboard any model or agent team onto HelixDrift quickly and
consistently.

This packet assumes the project is being worked by parallel teams across
`Codex`, `Claude`, and `Kimi`.

## Read These First

Every team should read these files before starting work:

1. `.agents/README.md`
2. `.agents/MODEL_ASSIGNMENT.md`
3. `.agents/PROMPTS.md`
4. `docs/AGENT_WORKFLOW.md`
5. `docs/DEV_JOURNAL_POLICY.md`
6. `docs/PR_CHECKLIST.md`
7. `TASKS.md`
8. `docs/SIMULATION_BACKLOG.md`
9. `docs/NRF52_SELECTION.md`

If the team is assigned RF or pose work, also read:

- `.agents/teams/rf-sync.md`
- `.agents/teams/pose-inference.md`

## Project Summary

HelixDrift is a simulation-first wearable mocap node project.

The current product intent is:

- a small body-worn sensor node
- IMU + magnetometer + barometer sensor assembly
- low-latency wireless synchronization to a master node
- host-side proof first, hardware second

Current project reality:

- automated validation is host-only
- simulation and host tests are the current proof mechanism
- intended primary MCU direction is `nRF52`, with `nRF52840` as the current
  preferred first target
- platform-specific implementation is secondary to proving the system in
  simulation

## Mandatory Rules

All teams must follow:

1. Design first.
2. Then TDD.
3. Then implementation and testing.
4. Then fix issues from tests and review.
5. Update the development journal for each feature.
6. Get at least 3 peer review rounds before merge.
7. Update all relevant docs before PR and merge.
8. Merge only when CI passes.
9. Use `nix develop`. Do not install project tools locally.

## Coding Guidance

Apply the imported global coding rules already reflected in `.agents/README.md`:

- C++17 only
- embedded-first style for firmware/common code
- no dynamic allocation, exceptions, or RTTI in embedded code
- GoogleTest / GoogleMock for tests
- explicit, host-testable design favored over platform entanglement

## Team Assignment Process

For each model:

1. choose the assigned team from `.agents/MODEL_ASSIGNMENT.md`
2. open the matching worktree
3. use the matching prompt from `.agents/PROMPTS.md`
4. stay within the assigned write scope
5. produce a handoff using `.agents/HANDOFF_TEMPLATE.md`

## Worktree Mapping

Recommended worktrees:

- `.worktrees/architect`
- `.worktrees/sensors`
- `.worktrees/fusion`
- `.worktrees/rf-sync`
- `.worktrees/nrf52`
- `.worktrees/host-tools`
- `.worktrees/pose-inference`

Create them with:

```bash
./tools/agents/create_worktrees.sh
```

## Current High-Value Focus

Teams should prioritize these questions:

1. Is each sensor independently proven?
2. Is the three-sensor assembly proven under known motion?
3. What outputs are actually required for the downstream skeleton?
4. What timing and sync model should node-to-master communication follow?
5. What should be implemented next to reduce hardware risk fastest?

## Recommended Active Teams Now

The current best set of active teams is:

- `Codex / Sensor Validation`
- `Codex / Fusion And Kinematics`
- `Codex / Host Tools And Evidence`
- `Claude / Systems Architect`
- `Claude / Pose Inference`
- `Kimi / RF/Sync Research`

## Team-Specific Launch Template

Copy this template and fill in the bracketed fields for each team.

```text
You are joining the HelixDrift project as [MODEL] / [TEAM].

Before doing any work, read:
- .agents/README.md
- .agents/MODEL_ASSIGNMENT.md
- .agents/PROMPTS.md
- docs/AGENT_WORKFLOW.md
- docs/DEV_JOURNAL_POLICY.md
- docs/PR_CHECKLIST.md
- TASKS.md
- docs/SIMULATION_BACKLOG.md
- docs/NRF52_SELECTION.md

Your assigned worktree is:
- [WORKTREE_PATH]

Your mission is:
- [TEAM_MISSION]

Your write scope is:
- [WRITE_SCOPE]

Rules:
- design first
- TDD first where practical
- implement only after tests or test updates
- use nix develop only
- update the development journal
- update relevant docs
- request at least 3 peer review rounds before merge
- do not edit outside scope unless explicitly required and documented

Your first task is:
- [FIRST_TASK]

Your final output for each work item must include:
- what changed
- tests run
- docs updated
- dev journal updated
- risks and open questions
- review requests needed
```

## Example Launches

### Example: Codex / Sensor Validation

```text
You are joining the HelixDrift project as Codex / Sensor Validation.

Read the bootstrap packet and all required docs first.

Your assigned worktree is:
- .worktrees/sensors

Your mission is:
- prove each sensor independently before fusion is trusted
- improve simulator fidelity and standalone driver validation

Your write scope is:
- simulators/i2c/
- simulators/sensors/
- simulators/tests/test_lsm6dso_simulator.cpp
- simulators/tests/test_bmm350_simulator.cpp
- simulators/tests/test_lps22df_simulator.cpp

Your first task is:
- create or refine the per-sensor validation matrix and identify the highest
  value missing quantitative test coverage
```

### Example: Claude / Pose Inference

```text
You are joining the HelixDrift project as Claude / Pose Inference.

Read the bootstrap packet and all required docs first.

Your assigned worktree is:
- .worktrees/pose-inference

Your mission is:
- determine what spatial outputs the product actually needs
- decide whether orientation-only is enough for v1
- clarify relative translation vs joint-angle recovery vs global position

Your write scope is:
- docs/
- .agents/

Your first task is:
- write a short requirements note defining what the downstream skeleton or
  control system actually needs from the nodes
```

### Example: Kimi / RF-Sync Research

```text
You are joining the HelixDrift project as Kimi / RF-Sync Research.

Read the bootstrap packet and all required docs first.

Your assigned worktree is:
- .worktrees/rf-sync

Your mission is:
- explore node-to-master communication and timing strategies
- compare low-latency RF options and synchronization models

Your write scope is:
- docs/
- RF/sync design notes

Your first task is:
- produce a structured comparison of plausible low-latency communication and
  synchronization approaches for wearable mocap nodes using nRF52-class MCUs
```

## Handoff Requirement

Every completed work item must be handed off using:

- `.agents/HANDOFF_TEMPLATE.md`

No team should treat work as complete until the handoff is written.
