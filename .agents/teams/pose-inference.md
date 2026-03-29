# Pose Inference Team Charter

## Mission

Determine what spatial information HelixDrift actually needs to reconstruct
useful human or animal motion, and identify the minimum signals required to
deliver that result credibly.

This team exists because orientation, joint angles, relative translation, and
global position are different problems. The product should not assume it needs
all of them.

## Core Questions

1. What outputs are actually required by the computer-side animation or
   control stack?
2. Is per-segment orientation sufficient for the intended skeleton?
3. When is relative translation required, and at what accuracy?
4. Can body kinematic constraints recover usable motion without explicit node
   positioning?
5. If translation is required, what additional observables would be needed?

## Deliverables

### D1. Output Requirements Note

Define the downstream outputs in concrete terms:

- segment orientation
- joint angle
- root orientation
- root translation
- per-node relative translation
- global body position

State which are required, optional, or out of scope for v1.

### D2. Feasibility Matrix

Compare these approaches:

- orientation-only per segment
- orientation plus fixed bone lengths
- orientation plus gait/contact constraints
- IMU dead reckoning
- barometer-assisted height changes
- RF-based ranging
- UWB
- vision-assisted alignment

For each, describe:

- what it can estimate
- expected drift/failure mode
- complexity
- simulation requirements

### D3. Recommendation

Make a concrete recommendation for v1, for example:

- v1 uses orientation-only segment tracking and kinematic skeleton fitting
- relative translation is deferred
- root position requires external reference or later additional hardware

### D4. Simulation Plan

Define what experiments are needed to validate the recommendation:

- joint-angle recovery
- multi-segment chain reconstruction
- gait or limb-cycle tests
- error sensitivity to orientation drift and timing skew

## Non-Goals

- Do not design the RF timing protocol.
- Do not assume custom RF provides spatial localization.
- Do not over-commit to absolute position without evidence.

## Recommended Write Scope

- `/home/mrumoy/sandbox/embedded/HelixDrift/docs/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/.agents/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/`

## First Task List

1. Write `docs/pose-inference-requirements.md`.
2. Write `docs/pose-inference-feasibility.md`.
3. Define v1 and v2 product assumptions for skeleton reconstruction.
4. Specify the first two kinematic simulation experiments to implement.
5. Hand off required simulator capabilities to the Fusion Team.

## Success Criteria

- The product no longer has ambiguous language around "position."
- The repo has a defensible v1 mocap output target.
- Simulation work focuses on the quantities that actually matter to the
  product rather than chasing unconstrained inertial position.
