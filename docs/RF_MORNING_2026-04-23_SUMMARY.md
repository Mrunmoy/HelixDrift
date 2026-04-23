# RF morning summary — 2026-04-23 (after Claude's autonomous session)

User woke up around 07:20. Claude continued working autonomously
on the items queued for morning + new ones the user added.

## What got done this morning

### 1. CI fix + code review (task #47)

- **Root cause of CI failure:** stale submodule pointer. Local
  `external/SensorFusion` referenced commits that were never
  pushed to the SensorFusion remote. Fixed by pushing the missing
  commits to `Mrunmoy/SensorFusion` main.
- **Unit test fix:** `OtaManagerTest.BeginFailsIfAlreadyReceiving`
  was stale after intentional behaviour change in commit 4c53235
  ("silent OTA bug" fix). Renamed + rewrote to match current
  contract.
- Both fixes committed as `c573335` + `5e0c1c7`. CI green.

### 2. Three rounds of code review by Codex + Copilot (tasks #47, #48)

User directive: "have them do multiple rounds of reviews until
they are happy. … In general I would like 3 rounds of reviews
and fixes."

- **Round 7** (code review on tonight's RF work): both flagged
  Hub anchor-queue phantom entries + midpoint volatile. All
  fixed (`c573335`).
- **Round 8** (follow-up on round 7 fixes): Codex found the CI
  reachability check was flawed (`ls-remote` only sees ref tips,
  not arbitrary ancestors). Copilot flagged weak OTA test body.
  Both fixed (`5b5058e`) — CI now uses `git fetch --depth=1
  <url> <sha>`, test actually exercises the partial-state-clear
  contract.
- **Round 9** (final sign-off): **Both reviewers "CLEAN - no
  further concerns"** (`930869f`).

All nine review-round artefacts persisted under
`docs/reviews/round{7,8,9}_*.md` and
`{codex,copilot}_2026-04-23_round{7,8,9}.md`.

### 3. Cross-Tag span fat-tail investigation (task #49)

Per-Tag |err| p99 is 5–10 ms but cross-Tag span p99 is ~42 ms.
5× gap investigation done:

- Built `tools/analysis/find_span_outliers.py`.
- Applied to 4.2 M-row overnight soak: found 4 184 bins with
  span > 30 ms (4.9 %, matches p99).
- Outliers distributed roughly uniformly across all 8 healthy
  Tags (~500 each). Individual Tag excursions: ±23–32 ms.
- **Diagnosis:** ~1.8 % of Stage 3 midpoint updates produce
  jumps ≥ 10 ms. Over 4 h × 0.9 Hz = ~233 large jumps/Tag,
  matching the 470–680 outlier bins observed.
- **Hypothesis:** residual cross-contamination bleed-through
  where a non-match anchor's `rx_node_id` byte coincidentally
  matches (1/10 chance).
- **Proposed fix (not implemented):** glitch-reject midpoint
  jumps > 10 ms. Expected to drop cross-Tag p99 from ~42 ms to
  ~5 ms.
- Doc: `docs/RF_CROSS_TAG_SPAN_INVESTIGATION.md`.

### 4. docs/RF.md §7 addendum (task #50)

Master consolidated RF doc now has:
- §7.11 Stage 3.7 retry instrumentation
- §7.12 Stage 4 Path X1 (v19/v20)
- §7.13 FIFO=1 discovery (the architectural breakthrough)
- §7.14 v21 A/B Stage 4 off wins
- §7.15 Overnight soak validation
- §7.16 Cross-Tag span investigation
- §9 rewritten to reflect Stage 4 DELETED

Commit `d788e5b`.

### 5. Stage 4 TDMA fate brainstorm + delete (task #51)

User directive: "(6) needs to be brainstormed with codex and
copilot teams of experts before final decision."

- Asked both reviewers to assemble 4-role teams (embedded RF /
  protocol / product / maintenance) and return keep-vs-delete.
- **Both returned UNANIMOUS DELETE**: FIFO=1 invalidated Stage
  4's value prop, 28 % throughput cost for no sync gain, future
  TDMA (if ever) would want a different design (beacon-based).
- **Executed** the delete in `0ae662f`: −127 lines, Kconfig +
  node.conf + #if blocks + orphan SUMMARY fields all cleaned up.
- **Preserved** the implementation at git tag
  `stage4-tdma-path-x1-reference` (commit c442eb7) and archived
  design doc to `docs/archive/rf/RF_STAGE4_EXPLORATORY.md`.
- Brainstorm artefacts:
  `docs/reviews/{codex,copilot}_2026-04-23_stage4_fate.md`.

## 6. Glitch-reject experiment (v22, task #52) — hypothesis REFUTED

After morning wrap-up user said "start finishing up the rest".
Implemented the glitch-reject fix from §7.16: drop single-sample
midpoint jumps ≥ 10 ms, with 5-consecutive safety valve for
legitimate big shifts.

Fleet-OTA'd 9/10 Tags to v22. 15-min capture showed:

- Cross-Tag span p99 went from 42.5 → **47.5 ms** (worse).
- Tag 1 locked onto -14.6 ms bad baseline; Tag 3 stuck at +2.57 s.
- Glitch-reject ACTIVELY PREVENTED recovery — correct samples after
  a bad baseline look like "big jumps" and get rejected. The
  5-consecutive safety valve never triggered because interleaved
  near-baseline samples reset the streak.

**Hypothesis refuted.** Reverted glitch-reject in commit `07531ad`.
Full doc: `docs/RF_V22_FINDINGS.md`.

## 7. Drift hypothesis — offline replay (also REFUTED)

Round-10 reviewer consensus (Codex + Copilot × 2) after v22
failure: fat-tail is DRIFT-driven, not JUMP-driven. Proposed
offline replay with PC-side bias correction — if span p99
collapses, drift architecture validated.

Built `tools/analysis/offline_bias_replay.py`, ran on 200k-row
subset of overnight soak:

- Raw (no correction): p99 **42.5 ms**
- Fixed per-Tag bias: p99 **41.5 ms**
- Rolling-10 s bias vs fleet median: p99 **41.0 ms**

**Only 1.5 ms improvement.** Drift hypothesis also refuted.

Current working hypothesis: the fat-tail is **per-Tag single-Tag
tail events** (retry storms, radio glitches, ISR jitter). Rare,
per-frame, irreducible on shared-pipe ESB without architecture
change. Full doc: `docs/RF_DRIFT_HYPOTHESIS_REFUTED.md`.

## Fleet state (as of checkpoint `rf-sprint-checkpoint-v1`)

- **Hub:** Stage 3.6 + `CONFIG_ESB_TX_FIFO_SIZE=1` (central.conf).
- **Fleet v22 source:** Stage 4 code removed, glitch-reject
  REVERTED to v21 behaviour. Ready for fleet OTA to v23 when
  user says go.
- **Fleet runtime:** Tags 1, 2, 4, 5, 6, 7, 9 on v22 (post-glitch-
  reject OTA). Tag 3 stuck at +2.57 s bias on v22. Tags 8, 10
  stuck on broken v19.
- **Source ≠ fleet:** commit `07531ad` onward has glitch-reject
  reverted in source, but fleet still runs v22 with the reject.
  No functional hazard — counters increment but behaviour is
  unchanged at this point in code. Fleet OTA to v23 would sync
  source and runtime.

## CI health

All runs since the fix green (through commit `35f29be`). Submodule
pre-check + stricter build-failure detection in `fleet_ota.sh`
protect against future classes of silent CI breakage.

## What's left — decision buckets for user

(Summary; full discussion in `docs/RF.md` §8.)

### Bucket 1 — Quick fleet hygiene (autonomous-safe)
- Fleet OTA to v23 (sync source with fleet).
- Tag 3 power-cycle + Tags 8, 10 SWD recovery (task #45).

### Bucket 2 — Ship v1 at current spec
- Per-Tag \|err\| p99 6–10 ms, cross-Tag span p99 42 ms, 429 Hz
  aggregate. Usable for slow mocap; marginal for fast motion.
- Merge `nrf-xiao-nrf52840` → `main`, tag `v1.0`.

### Bucket 3 — Architecture change for sub-ms
| Option | Effort |
|---|---|
| 3a Hardware TX timestamping (PPI + TIMER) | 3–5 days |
| 3b Per-Tag ESB pipes (≤8 Tags/Hub) | 1–2 days + product call |
| 3c TDMA with hardware Hub beacon (needs 3a) | 5–7 days |

### Cross-bucket: firmware hygiene (round-10 unanimous)
- Initial-lock qualification (N=5 samples within ±3 ms). ~2 h.
- Lock-health telemetry in SUMMARY. ~2 h.

**Task tracker pending items:**
| # | Task | Status |
|---|---|---|
| 45 | Tags 3/8/10 physical recovery | Needs bench access |
| 35 | Phase E per-Hub pipe derivation | Deferred (multi-Hub not needed 6 mo) |
| 21 | Parallel OTA | Parked |
| — | Merge branch → main | User decision |

## Commits today (most recent first)

```
35f29be  m8-rf: drift hypothesis ALSO refuted via offline replay
07531ad  m8-rf: v22 findings + revert glitch-reject (v23 = v21 behaviour)
c2c08db  m8-rf: v22 — midpoint glitch-reject (task #52)
96fb062  docs: RF morning 2026-04-23 session summary
04cd918  docs(RF.md): §9 — Stage 4 DELETED, archived references
0ae662f  m8-rf: delete Stage 4 TDMA code (v22)
da51c49  docs(reviews): Stage 4 TDMA fate brainstorm — UNANIMOUS DELETE
d788e5b  docs(RF.md): §7.11-7.16 addendum for v18-v21 + FIFO=1 history
12102c1  docs+tools: cross-Tag span fat-tail investigation
930869f  docs(reviews): round 9 final — both reviewers CLEAN
5b5058e  fix(ci+test): round 8 reviewer follow-ups
5e0c1c7  docs(reviews): round 7 — code review on overnight RF work
c573335  fix(ci+rf): apply Codex+Copilot review + make CI pass
```

Tags: `rf-sprint-checkpoint-v1` (this state),
`stage4-tdma-path-x1-reference` (last Stage 4 code state).

Plus tag `stage4-tdma-path-x1-reference` at `c442eb7`.

All pushed to `origin/nrf-xiao-nrf52840`.
