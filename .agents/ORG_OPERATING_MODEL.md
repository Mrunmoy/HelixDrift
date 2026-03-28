# Org Operating Model

This document defines how the three model orgs (`Codex`, `Claude`, `Kimi`)
operate in parallel without creating merge chaos.

## Core Principle

Parallelism is only safe when ownership is explicit.

The safest default is:

- one primary owner per directory or file set
- other orgs review or propose, but do not edit those files concurrently
- all subteam changes merge into an org integration worktree first
- only then are org-integrated changes merged into the repo integration branch

## Merge Chaos Prevention Rules

1. No concurrent writes to the same file by multiple active teams.
2. No concurrent writes to the same directory by multiple orgs unless the work
   is explicitly partitioned by file and recorded in the org status doc.
3. Reviews should be comment-only by default.
4. Research teams should write to docs, not implementation files, unless they
   are explicitly assigned code ownership for a task.
5. Each org consolidates its own subteams before handing work upward.
6. The top-level integrator merges org outputs only after org-level
   consolidation is complete.

## Safe Ownership Strategy

Prefer this order of isolation:

1. separate directories
2. if same directory is unavoidable, separate files
3. if same file is unavoidable, work must become sequential rather than parallel

## Primary Ownership Matrix

### Codex Org

Default implementation owner for:

- `simulators/i2c/`
- `simulators/sensors/`
- `simulators/gimbal/`
- `simulators/tests/`
- `tests/`
- `tools/`
- `examples/nrf52-mocap-node/`
- `firmware/common/` where implementation work is explicitly assigned

### Claude Org

Default design and architecture owner for:

- `docs/`
- `.agents/`
- `TASKS.md`

Claude should usually avoid editing implementation code unless the task is
explicitly architecture glue or doc-driven interface cleanup.

### Kimi Org

Default exploratory research owner for:

- `docs/research/` when created
- `.agents/orgs/kimi/`
- research notes under `docs/`

Kimi should usually avoid direct implementation edits.

## Review-Only By Default

These org behaviors are preferred:

- Claude reviews Codex code without editing it unless invited
- Kimi reviews or challenges design notes without editing implementation
- Codex reviews implementation feasibility of Claude/Kimi proposals without
  rewriting their research docs unless assigned

## Org Consolidation Flow

Each org should have:

- one org lead worktree
- multiple subteam worktrees
- one org integration worktree

Flow:

1. subteams work in parallel
2. subteams hand off to org lead
3. org lead resolves internal conflicts in the org integration worktree
4. org lead updates org status docs
5. top-level integrator merges the org integration result

## Directory Recommendations

To keep the separation clean:

- Codex implementation artifacts live mostly in code directories
- Claude design artifacts live in `docs/` and `.agents/`
- Kimi exploratory artifacts should be placed under a dedicated research area

Recommended future directory:

- `docs/research/`

Suggested subdirectories later:

- `docs/research/rf-sync/`
- `docs/research/pose/`
- `docs/research/hardware/`

## Escalation Rule

If a task would force two orgs to edit the same file in parallel:

1. stop
2. record the conflict in the org status doc
3. assign one org as the temporary owner
4. make the other org review-only for that file

## Required Org Tracking

Each org must keep:

- active teams
- owned scopes
- blocked scopes
- files currently being edited
- pending reviews
- integration status

Use the org status docs under `.agents/orgs/`.
