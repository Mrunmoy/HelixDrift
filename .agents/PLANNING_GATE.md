# Planning Gate

Planning is mandatory before execution.

The purpose of this gate is to prevent:

- duplicated work
- overlapping write scopes
- wasted model tokens
- unnecessary merge conflict resolution
- parallel implementation of unresolved design questions

## Core Rule

No team starts coding until the planning gate is satisfied.

This includes:

- subteams inside an org
- org leads
- single-session orgs
- Claude internal worker teams if Claude spawns them

## What Must Exist Before Execution

Each org must first record, in its org status doc:

1. active teams
2. exact mission for each active team
3. exact writable scope for each active team
4. exact review-only scope for each active team
5. blocked or contested scopes
6. handoff path
7. no-duplication check

## No-Duplication Check

Before starting work, every org lead must confirm:

- no other active team is solving the same problem
- no other active team is editing the same writable scope
- if another team is working the same topic, the relationship is explicit:
  - owner
  - reviewer
  - challenger
  - deferred follower

If this cannot be stated clearly, execution must not begin.

## Problem Ownership Rule

Ownership is by problem first, then by files.

Examples:

- "IMU standalone validation" is one problem
- "pose-inference v1 requirements" is one problem
- "RF option comparison" is one problem
- "timestamp sync host harness" is one problem

Only one active owner should exist for a problem at a time.

Other teams may:

- review it
- challenge it
- prepare downstream work

but should not duplicate the same execution.

## Scope Resolution Order

Resolve planning conflicts in this order:

1. separate by problem
2. then separate by directory
3. then separate by file
4. if still overlapping, force sequential work instead of parallel work

## Org Lead Responsibilities

Before saying "go" to any subteam, the org lead must:

1. publish the org plan
2. mark claimed scopes
3. mark review-only scopes
4. identify likely handoffs
5. identify expected peer reviewers
6. confirm no-duplication

## Top-Level Integration Check

Before repo-wide execution begins, compare:

- `.agents/orgs/codex/ORG_STATUS.md`
- `.agents/orgs/claude/ORG_STATUS.md`
- `.agents/orgs/kimi/ORG_STATUS.md`

If overlaps exist:

- reassign ownership
- record the decision
- then start work

## Minimum Planning Output

Every org plan should answer:

1. what exact problems are we solving now?
2. who owns each problem?
3. which files or directories may be edited?
4. which files or directories are review-only?
5. what work is intentionally deferred?
6. what reviews are expected before merge?

## Rule For Shared Topics

Some topics are shared at the discussion level but must not be duplicated at
the execution level.

Example:

- Kimi may own RF/sync research
- Claude may review and structure those findings later
- Codex may implement the chosen direction later

That is acceptable because the roles differ.

What is not acceptable is:

- Kimi and Claude both independently drafting competing primary RF/sync plans
  at the same time without explicit intent

## Execution Start Condition

A team may start work only when:

- its mission is written down
- its scope is written down
- its owner is clear
- its no-duplication check is complete
- the org lead has approved the plan
