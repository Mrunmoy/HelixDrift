
If observed effect is smaller, the fat-tail is not primarily
driven by midpoint jumps — maybe hypothesis (3) radio glitches.

## Action

Not fixing autonomously — per user's sprint directive:
1. Reviewers will sanity-check this diagnosis + fix proposal.
2. If agreed, land glitch-reject as v22.
3. Measure. Commit findings.

Tool: `tools/analysis/find_span_outliers.py` (committed alongside
this doc). Re-run against any future capture to check if cross-Tag
outliers persist.

codex
**Recommendation: delete Stage 4 Path X1 runtime code now; archive the learning, not the feature.**

**Consensus**
Embedded RF engineer: FIFO=1 fixed the actual measured failure mode. Stage 4 Path X1 did not produce true slot isolation because its timing reference came from the same biased/noisy midpoint path it was trying to improve. Keeping a `k_usleep`-based slot-align path invites a future regression: lower throughput with no proven sync gain.

Protocol architect: the salvageable artifact is architectural knowledge: “deep ACK FIFO corrupts midpoint symmetry” and “TDMA needs an independent beacon or hardware TX timestamp, not self-referential midpoint scheduling.” The current implementation is not the future TDMA design; it is a falsified experiment.

Product owner: v21 A/B is decisive enough. Default-off code that imposes cognitive load but no product value should not stay in the active tree. Preserve the evidence so the decision is auditable.

Maintenance engineer: delete. Kconfig-gated dead experiments rot, confuse telemetry, and multiply test surfaces. “Already paid” only applies if the code remains aligned with the product direction; this one encodes an obsolete path.

**A. Salvageable vs. Delete**
Salvage the lesson and maybe a short archived design note. Delete the implementation. The lock state machine is interesting, but in this form it is coupled to Stage 4 TDMA behavior and SUMMARY fields. If a generic sync-confidence signal is needed later, rebuild it intentionally around the v21/v22 estimator, not by preserving a Stage 4-shaped abstraction.

**B. Cross-Tag Fat Tail**
The proposed midpoint glitch-reject is mostly orthogonal to Stage 4 Path X1. A *real* TDMA system with independent beacon/hardware timing could reduce bleed-through by preventing overlapping Tag traffic and FIFO contamination. But the implemented Path X1 did not fundamentally change that mechanism, because slot timing was derived from the contaminated midpoint and did not actually isolate airtime. Glitch-reject is the right next low-risk fix.

**C. Future Scaling**
Yes, >10 Tags or multi-Hub may eventually need TDMA again. But you would not want to resurrect this exact code. Future TDMA should be timer/beacon-driven, probably Hub-clock authoritative, with explicit acquisition/loss semantics and measured air-slot guarantees. Deleting now is unlikely to cause regret if `RF_STAGE4_EXPLORATORY.md` or an archived successor records the findings and the “do not repeat Path X1” constraint.

**D. Safe Unwind**
Delete is safe if done cleanly:

- Remove `HELIX_STAGE4_*` Kconfig entries.
- Remove `#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)` blocks from `main.cpp`.
- Remove the Stage 4 block from `node.conf`.
- Move `docs/RF_STAGE4_EXPLORATORY.md` to `docs/archive/rf/`, preferably with a short “superseded by FIFO=1 discovery” header.
- Remove or rename orphan SUMMARY fields so logs do not imply an inactive feature still matters.
- Keep `docs/RF.md` history sections or add a concise tombstone note pointing to the archive.

**Single-line decision:** delete Stage 4 Path X1 from active firmware, archive the experiment, and only reintroduce TDMA later as a fresh beacon/timer-based design if scaling or fusion data proves it necessary.
tokens used
37,678
**Recommendation: delete Stage 4 Path X1 runtime code now; archive the learning, not the feature.**

**Consensus**
Embedded RF engineer: FIFO=1 fixed the actual measured failure mode. Stage 4 Path X1 did not produce true slot isolation because its timing reference came from the same biased/noisy midpoint path it was trying to improve. Keeping a `k_usleep`-based slot-align path invites a future regression: lower throughput with no proven sync gain.

Protocol architect: the salvageable artifact is architectural knowledge: “deep ACK FIFO corrupts midpoint symmetry” and “TDMA needs an independent beacon or hardware TX timestamp, not self-referential midpoint scheduling.” The current implementation is not the future TDMA design; it is a falsified experiment.

Product owner: v21 A/B is decisive enough. Default-off code that imposes cognitive load but no product value should not stay in the active tree. Preserve the evidence so the decision is auditable.

Maintenance engineer: delete. Kconfig-gated dead experiments rot, confuse telemetry, and multiply test surfaces. “Already paid” only applies if the code remains aligned with the product direction; this one encodes an obsolete path.

**A. Salvageable vs. Delete**
Salvage the lesson and maybe a short archived design note. Delete the implementation. The lock state machine is interesting, but in this form it is coupled to Stage 4 TDMA behavior and SUMMARY fields. If a generic sync-confidence signal is needed later, rebuild it intentionally around the v21/v22 estimator, not by preserving a Stage 4-shaped abstraction.

**B. Cross-Tag Fat Tail**
The proposed midpoint glitch-reject is mostly orthogonal to Stage 4 Path X1. A *real* TDMA system with independent beacon/hardware timing could reduce bleed-through by preventing overlapping Tag traffic and FIFO contamination. But the implemented Path X1 did not fundamentally change that mechanism, because slot timing was derived from the contaminated midpoint and did not actually isolate airtime. Glitch-reject is the right next low-risk fix.

**C. Future Scaling**
Yes, >10 Tags or multi-Hub may eventually need TDMA again. But you would not want to resurrect this exact code. Future TDMA should be timer/beacon-driven, probably Hub-clock authoritative, with explicit acquisition/loss semantics and measured air-slot guarantees. Deleting now is unlikely to cause regret if `RF_STAGE4_EXPLORATORY.md` or an archived successor records the findings and the “do not repeat Path X1” constraint.

**D. Safe Unwind**
Delete is safe if done cleanly:

- Remove `HELIX_STAGE4_*` Kconfig entries.
- Remove `#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)` blocks from `main.cpp`.
- Remove the Stage 4 block from `node.conf`.
- Move `docs/RF_STAGE4_EXPLORATORY.md` to `docs/archive/rf/`, preferably with a short “superseded by FIFO=1 discovery” header.
- Remove or rename orphan SUMMARY fields so logs do not imply an inactive feature still matters.
- Keep `docs/RF.md` history sections or add a concise tombstone note pointing to the archive.

**Single-line decision:** delete Stage 4 Path X1 from active firmware, archive the experiment, and only reintroduce TDMA later as a fresh beacon/timer-based design if scaling or fusion data proves it necessary.
