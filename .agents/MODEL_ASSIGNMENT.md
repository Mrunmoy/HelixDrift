# Multi-Model Team Structure

This document defines how `Codex`, `Claude`, and `Kimi` should work on
HelixDrift in parallel.

It is written so you can hand the same repo context to each model and assign
the right work to the right team while keeping the global rules consistent.

## Global Rules

All models and all teams must follow the repo-global rules defined in:

- `.agents/README.md`
- `docs/AGENT_WORKFLOW.md`
- `docs/DEV_JOURNAL_POLICY.md`
- `docs/PR_CHECKLIST.md`

Non-negotiable workflow:

1. Design first.
2. Then TDD.
3. Then implementation and testing.
4. Then fix issues found in tests and review.
5. Update the development journal for each feature.
6. Get at least 3 peer review rounds before merge.
7. Update all relevant docs before PR and merge.
8. Merge only when CI passes.
9. Use `nix develop`, never local ad hoc installs.

## Model Roles

### Codex

Best used for:

- implementation-heavy tasks
- test creation and maintenance
- code refactors
- harnesses, utilities, build, and CI wiring
- bounded engineering tasks tied closely to the repo

### Claude

Best used for:

- architecture
- requirements shaping
- design notes
- feasibility analysis
- structured review and decision support

### Kimi

Best used for:

- broad exploration
- design-space search
- alternative solution generation
- adversarial challenge of assumptions
- early-stage research where multiple candidate paths are needed

## Organization Structure

Use the three models as three parallel orgs.

Each org can have one or more teams. Teams should not duplicate the same work
unless the goal is intentional design comparison or review.

## Codex Org

### Codex Team 1: Sensor Validation

Mission:
- prove each sensor independently
- improve simulator fidelity
- expand per-sensor quantitative tests

Primary scope:
- `simulators/i2c/`
- `simulators/sensors/`
- per-sensor tests

Why Codex:
- this is code-heavy, test-heavy, and tightly coupled to the current repo

### Codex Team 2: Fusion And Kinematics

Mission:
- validate the three-sensor assembly
- improve fusion accuracy tests
- add motion-script and quantitative orientation regression tests

Primary scope:
- `simulators/gimbal/`
- `simulators/tests/test_sensor_fusion_integration.cpp`
- fusion-related common code

Why Codex:
- this is implementation-heavy and benefits from direct repo-aware iteration

### Codex Team 3: Host Tools And Evidence

Mission:
- create CSV export, plotting, and simulator evidence tooling
- make it easy to inspect runs and compare truth vs output

Primary scope:
- `tools/`
- simulation harness utilities
- validation formatting helpers

Why Codex:
- this is concrete tooling work with straightforward implementation scope

### Codex Team 4: nRF52 Platform

Mission:
- stabilize the nRF52 example path
- keep platform code thin and ready for future hardware
- align platform interfaces with validated common runtime

Primary scope:
- `examples/nrf52-mocap-node/`
- `tools/nrf/`

Why Codex:
- this is implementation and repo-integration work

## Claude Org

### Claude Team 1: Systems Architect

Mission:
- own roadmap integrity
- maintain system interfaces and backlog clarity
- identify missing design decisions and integration risks

Primary scope:
- `docs/`
- `.agents/`
- `TASKS.md`

Why Claude:
- this requires strong structure, decomposition, and decision framing

### Claude Team 2: Pose Inference

Mission:
- define what outputs the product actually needs
- decide whether orientation-only is enough for v1
- clarify relative translation vs joint-angle recovery vs global position

Primary scope:
- `docs/`
- pose and kinematics design notes

Why Claude:
- this is primarily a requirements and system-definition problem

### Claude Team 3: Review Board

Mission:
- perform peer reviews across orgs
- focus on design coherence, scope control, and verification quality

Primary scope:
- review only unless explicitly asked to revise docs

Why Claude:
- strong at structured critique and identifying weak assumptions

## Kimi Org

### Kimi Team 1: RF/Sync Research

Mission:
- explore protocol options
- compare BLE, proprietary 2.4 GHz, and other low-latency node-to-master
  communication approaches
- pressure-test timing assumptions

Primary scope:
- `docs/`
- RF research and comparison notes

Why Kimi:
- useful for broad option generation and solution-space exploration

### Kimi Team 2: Pose Feasibility Research

Mission:
- explore what is realistically inferable from IMU + magnetometer + barometer
- identify limits of inertial-only relative positioning
- suggest external references or constraints if needed

Primary scope:
- `docs/`
- feasibility notes and experiment proposals

Why Kimi:
- useful when exploring multiple candidate methods and tradeoffs

### Kimi Team 3: Hardware Futures

Mission:
- explore future product directions:
  - sensor stack alternatives
  - power and packaging tradeoffs
  - radio architecture alternatives
  - later-stage hardware options

Primary scope:
- `docs/`

Why Kimi:
- useful for future-looking broad option analysis

### Kimi Team 4: Adversarial Reviewer

Mission:
- challenge assumptions
- identify blind spots
- highlight where the current plan might fail in the real world

Primary scope:
- review only unless asked for design notes

Why Kimi:
- useful as a challenge function rather than a primary implementer

## Recommended Active Teams Right Now

Start with these six active teams:

1. `Codex / Sensor Validation`
2. `Codex / Fusion And Kinematics`
3. `Codex / Host Tools And Evidence`
4. `Claude / Systems Architect`
5. `Claude / Pose Inference`
6. `Kimi / RF/Sync Research`

This gives:

- concrete code progress
- architecture guidance
- product-definition progress
- RF exploration in parallel

without creating too many simultaneous merge paths.

## Work Allocation Pattern

For most features, use this pattern:

1. one model explores or designs
2. one model implements
3. one different model reviews

Example:

- Kimi explores protocol options
- Claude turns the best option into a concrete design note
- Codex implements the host-side harness
- Claude reviews for architecture
- Kimi reviews for missing alternatives or risk
- Codex fixes and hands off

## Review Routing

Use at least three review rounds.

Suggested routing:

- Codex implementation:
  - Claude review
  - Kimi adversarial review
  - Systems Architect or owning adjacent team final review

- Claude design note:
  - Kimi challenge review
  - Codex implementation-feasibility review
  - Systems Architect signoff

- Kimi research note:
  - Claude structure and requirements review
  - Codex implementation-feasibility review
  - Systems Architect signoff

## Team Launch Guidance

When starting any team:

- give it the matching prompt from `.agents/PROMPTS.md`
- remind it of the global workflow and coding rules
- keep its write scope narrow
- require a handoff using `.agents/HANDOFF_TEMPLATE.md`

## Do Not Do This

- do not have all three models implement the same code task in parallel
- do not let multiple teams edit the same files at once unless one is
  intentionally review-only
- do not allow implementation before design and TDD expectations are addressed

## Immediate Suggested Assignments

### Codex

- Sensor Validation
- Fusion And Kinematics
- Host Tools And Evidence

### Claude

- Systems Architect
- Pose Inference
- Review Board

### Kimi

- RF/Sync Research
- Pose Feasibility Research
- Hardware Futures
- Adversarial Reviewer

## Project Reality Reminder

HelixDrift is currently:

- simulation-first
- host-test-first
- nRF52-first in intended platform direction

The current highest-value unresolved questions are:

1. proving each sensor independently
2. proving the three-sensor assembly
3. deciding what spatial outputs are actually required
4. defining low-latency timing and sync behavior

That is where the teams should focus now.
