# Agent Teams

This folder defines the local agent-team structure for HelixDrift.

The intent is to let one human operator run multiple parallel coding or
research agents against the same project with minimal merge conflict and clear
ownership boundaries.

## Global Rules

These rules apply to every agent team and every feature branch.

1. Design first.
2. Then TDD.
3. Then implement and run unit tests.
4. Then fix issues found during testing and review.
5. Keep the development journal updated for each feature.
6. Require at least 3 rounds of peer review by other agent teams before merge.
7. Fix review findings before merge.
8. Update all relevant documentation before opening or merging a PR.
9. Merge only when CI passes.
10. Use `nix develop` for tools and builds. Do not install project tooling
    locally on the machine.

## Imported Coding Guidelines

The reusable coding standards from `~/.claude/CLAUDE.md` apply here unless
project-specific guidance overrides them.

### C++ Baseline

- C++17 only. Do not introduce C++20 features.
- Embedded-first mindset in library or firmware code:
  - no dynamic allocation
  - no exceptions
  - no RTTI
  - no `std::vector`, `std::string`, or allocating containers in embedded code
- Test code and host-only tools may use STL containers where appropriate.
- Prefer compile-time validation and `constexpr` where practical.
- Use `[[nodiscard]]` on bool or error-returning functions when editing or
  introducing APIs.

### Naming

- Classes and structs: `PascalCase`
- Functions and methods: `camelCase`
- Private members: prefer project-local convention first; where not already
  established, use `m_` prefix only if the surrounding code uses it
- Constants: `kCamelCase`
- Namespaces: `snake_case` or short lowercase
- Macros: `SCREAMING_SNAKE_CASE`
- Template parameters: `PascalCase`

When HelixDrift already has a local naming convention, follow the local code.

### Formatting

- `#pragma once` for headers unless a C API header requires traditional guards
- 4-space indentation, no tabs
- Include order:
  - project headers
  - external headers
  - standard library headers
- Comments should explain why, not what

### Error Handling

- Prefer explicit return-status handling over exceptions
- Use compile-time checks with `static_assert` where appropriate

### Testing

- GoogleTest and GoogleMock are the default test tools
- TDD-first remains mandatory
- Prefer isolated, attributable tests over broad ambiguous tests

### Platform Rule

- Keep embedded-safe common code separate from platform-specific code
- Keep nRF52 as the primary platform narrative and target direction
- Keep ESP32-S3 work secondary unless specifically justified

## Required Workflow

For every non-trivial feature:

1. Write or update a design note first.
2. Add or update tests before implementation where practical.
3. Implement in the narrowest owned scope possible.
4. Run the relevant host-side tests.
5. Update the development journal entry for the feature.
6. Request peer review from at least three agent teams.
7. Address findings and re-run tests.
8. Update docs and backlog as needed.
9. Merge only after CI is green.

## Development Journal

Every feature should leave an audit trail in the development journal.

Minimum journal entry contents:

- feature name
- intent
- design decision summary
- tests added or changed
- implementation summary
- review rounds and findings
- final verification status

## Core Rule

Each agent team gets:

- a narrow mission
- an explicit write scope
- a separate git worktree
- a handoff file for findings and integration notes

Do not let multiple agent teams edit the same files at the same time unless one
agent is read-only.

## Important Systems Note

Custom RF protocol work is necessary for latency and time synchronization, but
RF comms alone will not tell you where each node is in space.

With IMU + magnetometer + barometer you can usually estimate orientation much
more reliably than position. Position or node-to-node translation will drift
unless you add stronger constraints or references such as:

- body kinematic constraints and fixed segment lengths
- contact events and motion priors
- external anchors such as UWB
- external reference systems such as vision

Therefore the system roadmap should be:

1. prove each sensor alone
2. prove the three-sensor assembly
3. prove per-node orientation
4. prove node-to-master time sync and low-latency transport
5. prove useful body-joint kinematics
6. only then decide how much true position estimation is realistic

## Recommended Agent Teams

### 1. Systems Architect

Mission:
- own product-level decisions and integration boundaries
- keep the repo aligned with the simulation-first roadmap
- break down work for other agents

Write scope:
- `TASKS.md`
- `docs/`
- `.agents/`

### 2. Sensor Validation Team

Mission:
- prove each sensor independently before fusion is trusted
- improve simulator fidelity and single-sensor driver validation

Write scope:
- `simulators/i2c/`
- `simulators/sensors/`
- `simulators/tests/test_lsm6dso_simulator.cpp`
- `simulators/tests/test_bmm350_simulator.cpp`
- `simulators/tests/test_lps22df_simulator.cpp`

### 3. Fusion And Kinematics Team

Mission:
- validate the three-sensor assembly
- improve orientation quality, drift behavior, and motion-script regression
- work toward body-joint estimation

Write scope:
- `simulators/gimbal/`
- `simulators/tests/test_sensor_fusion_integration.cpp`
- `firmware/common/MocapNodeLoop.hpp`
- `firmware/common/MocapProfiles.hpp`

### 4. RF And Sync Team

Mission:
- define node-to-master timing contracts
- simulate latency, jitter, packet loss, and anchor updates
- prototype the custom low-latency protocol shape

Write scope:
- `firmware/common/TimestampSynchronizedTransport.hpp`
- `firmware/common/MocapBleSender.hpp`
- `firmware/common/MocapBleSender.cpp`
- `tests/test_timestamp_synchronized_transport.cpp`
- `tests/` and `simulators/` for sync and transport harness additions

### 5. nRF52 Platform Team

Mission:
- make nRF52 the primary target platform
- keep target code thin and aligned with host-validated contracts
- own low-power platform assumptions

Write scope:
- `examples/nrf52-mocap-node/`
- `tools/nrf/`
- target-specific build wiring

### 6. Host Tools And Visualization Team

Mission:
- create host-side tools to inspect simulator output
- add CSV export, plotting hooks, and skeleton playback helpers
- make simulation evidence easy to review

Write scope:
- `tools/`
- new host analysis scripts or exporters
- `docs/validation/` for simulator-result formats

### 7. Pose Inference Team

Mission:
- determine whether node-relative translation is actually required for the
  product goal
- define what the computer needs from the nodes to drive a human or animal
  skeleton acceptably
- evaluate orientation-only mocap, kinematic-chain inference, and any added
  observables required for translation or global position

Write scope:
- `docs/`
- `.agents/`
- `simulators/` for future kinematic validation harnesses

This team is research-heavy first and implementation-heavy later.

Its first job is to answer:

- what minimum per-node outputs are required by the downstream skeleton system
- whether per-segment orientation plus fixed segment lengths is sufficient
- where true translation is required and where it is not
- what extra signals would be needed if translation accuracy must improve

## Worktree Model

Create one worktree per team from the current branch.

Suggested worktree names:

- `.worktrees/architect`
- `.worktrees/sensors`
- `.worktrees/fusion`
- `.worktrees/rf-sync`
- `.worktrees/nrf52`
- `.worktrees/host-tools`
- `.worktrees/pose-inference`

Use `tools/agents/create_worktrees.sh` to scaffold them.

## Operating Model

1. Run one terminal or tmux pane per worktree.
2. Run one coding agent session inside each worktree.
3. Give each agent only one mission and one write scope.
4. Keep one main integration session in the repo root.
5. Merge or cherry-pick finished work back through the integration session.
6. Use `nix develop` inside each worktree for builds, tests, and tooling.

## Suggested Prompts Per Team

Systems Architect:
"Own roadmap, backlog, and interface decisions. Do not edit simulator or
platform code except for minimal integration glue."

Sensor Validation Team:
"Own single-sensor simulator fidelity and standalone proof. Do not change
fusion or platform code unless a test harness requires a tiny interface shim."

Fusion And Kinematics Team:
"Own sensor-assembly and fused-pose validation. Do not edit low-level sensor
simulators except by filing a handoff request."

RF And Sync Team:
"Own timestamp alignment and protocol simulation. Do not edit platform code.
Model timing and transport behavior on host first."

nRF52 Platform Team:
"Treat nRF52 as the primary MCU target. Keep the runtime aligned with validated
common code. Avoid platform-specific logic in common layers."

Host Tools And Visualization Team:
"Own evidence generation. Make simulator results inspectable by humans."

Pose Inference Team:
"Determine what the mocap system actually needs to estimate. Separate
orientation, joint-angle recovery, relative translation, and global position.
Do not assume custom RF solves spatial localization. Produce concrete
recommendations and simulation experiments."

## Team Charters

Detailed charters live in:

- `.agents/teams/rf-sync.md`
- `.agents/teams/pose-inference.md`

Related workflow docs:

- `.agents/BOOTSTRAP_PACKET.md`
- `.agents/PLANNING_GATE.md`
- `.agents/ORG_OPERATING_MODEL.md`
- `.agents/OWNERSHIP_MATRIX.md`
- `docs/AGENT_WORKFLOW.md`
- `docs/DEV_JOURNAL_POLICY.md`
- `docs/PR_CHECKLIST.md`
- `docs/NRF52_SELECTION.md`
- `.agents/MODEL_ASSIGNMENT.md`
