# Development Journal Policy

Every feature must leave a trace in the development journal.

## Journal Locations

Until a repo-wide journal is added, use:

- `simulators/docs/DEV_JOURNAL.md` for simulator, fusion, sync, and host-test work
- future platform-specific journals may be added under target-specific docs

If a feature spans multiple areas, add the primary narrative to the most
relevant journal and cross-reference the other affected areas.

## Required Entry Fields

Each entry should capture:

- date
- feature or task name
- owning team
- intent
- design summary
- tests added or updated
- implementation summary
- review rounds completed
- final verification commands and results
- follow-up risks or open questions

## Entry Timing

Update the journal:

- when the design is agreed
- when implementation materially changes
- when review findings cause a change in direction
- before PR or merge

## Rule

No feature is considered complete if the code changed but the journal does not
explain why, how it was verified, and what remains uncertain.
