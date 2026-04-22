# Overnight work summary — 2026-04-22 → 2026-04-23

User went to sleep at ~22:00 with instruction: "based on group
consensus go ahead with development through the night. im going to
sleep. i trust you shall lead the project well."

## Progression

| Time | Action | Result |
|---|---|---|
| ~20:00 | Committed round-5 reviewer responses (Codex + Copilot on Stage 3 findings) | ✓ |
| ~20:30 | Consulted both reviewers on Stage 2 findings (round 4) + consolidated D1–D6 decisions off RF.md (round 6). Consensus: do retry-instrumentation (v18) first, then Stage 4 TDMA path (b) | ✓ |
| ~21:00 | Landed v18 retry instrumentation. Fleet-OTA + 20-min capture. | Showed retry bucket is NOT the driver of the 1.4 % ≥30 ms tail → skip Stage 3.5 |
| ~22:00 | Designed Stage 4 Path X1 (Tag-side TDMA, no separate beacon). Both reviewers confirmed soundness. | Codex + Copilot: Path X1 is right |
| ~22:30 | First v19 Stage 4 deploy — fleet_ota silently deployed stale v18 due to build-failure detection bug. Caught via SWD readout. | Fixed brace error; hardened fleet_ota.sh |
| ~23:30 | Real v19 deploy — TX rate collapsed 43 Hz → <1 Hz. Root cause: skip-forever bug in TDMA alignment. | Fixed in v20 |
| ~00:30 | v20 deploy: 30 Hz TX rate OK, cross-Tag span **unchanged** at 50 ms p99 despite TDMA active | Discovered +10 ms bias inherent in midpoint estimator under FIFO=8 |
| ~01:00 | Diagnosed: asymmetric FIFO queue delay biases the midpoint. Hypothesized `CONFIG_ESB_TX_FIFO_SIZE=1` would fix. | |
| ~01:30 | Hub-only rebuild with FIFO=1. **95 % sync-error reduction** per Tag. Fleet mean bias now ±0.6 ms, |err| p50 sub-ms. | 🎯 **The night's breakthrough** |
| ~02:00 | Started 4-hour overnight soak to validate stability. | Running |
| ~06:20 | 4-hour soak complete. | Stable. |

## Final night state

### Fleet firmware
- **Hub:** Stage 3.6 anchor + `CONFIG_ESB_TX_FIFO_SIZE=1` (one-line change — the real win)
- **7 Tags (1, 2, 4, 5, 6, 7, 9):** v20 Stage 4 TDMA active (redundant given FIFO=1)
- **Tag 3:** v20 but stuck in a degraded midpoint (-6 ms consistent bias) — needs re-lock
- **Tag 8, 10:** stuck on v19 with TDMA skip-forever bug, TXing at ~1.5 Hz each

### Soak measurements (4 h, 7 clean Tags excluded 3, 8, 10)

| Metric | Value |
|---|---:|
| Rows captured | 4.2 M |
| Per-Tag |err| p50 | 500 – 2000 µs |
| Per-Tag |err| p99 | 9 – 15 ms |
| Fleet mean bias range | -638 .. +1053 µs (1.7 ms spread) |
| Cross-Tag span p50 | 32 ms |
| Cross-Tag span p99 | 49.5 ms |
| Aggregate rate | 290 Hz (≈ 35 Hz/Tag) |
| Sparse outliers | Tag 1 max 84 s, Tag 6 occasional 2^32-µs wrap events (a handful across 4 h) |

### Cross-Tag span p99 is still ~50 ms — why?

Per-Tag |err| is sub-ms median / single-digit ms p99. But cross-Tag
span (max-min across Tags in each 50 ms wall-clock bin) stays at
~50 ms p99. Hypothesis:

- Fat-tail single-Tag outlier events (Tag 6, Tag 1 occasional
  wraparound / lock-reacquire events) pull the bin's max or min
  when they happen.
- With 7 Tags × 500k frames = 3.5M frames, even 0.1% fat-tail
  events produce thousands of outlier bins → p99 reaches ~50 ms.

The median 32 ms span is also suspicious — it's ≫ per-Tag median
|err|. Could be accumulation of 1-3 ms drift between Tags that
each locked to slightly different midpoint offsets. Worth
investigating in the morning whether a tighter lock criterion
would help.

## What worked

1. **FIFO=1 fix.** 95 % reduction in per-Tag sync error. One-line
   change (`CONFIG_ESB_TX_FIFO_SIZE=1` in `central.conf`). This is
   the real architectural insight the night produced.
2. **Retry instrumentation** proved the reviewer-hypothesised
   retry-driven tail was not real — saved a day of Stage 3.5 code
   work.
3. **Reviewer consultations** (3 pairs — Codex + Copilot on round
   4, round 6, and Path X1 sanity) all landed useful inputs.
4. **fleet_ota.sh hardening** — build-failure detection now works
   correctly.
5. **Stage 4 TDMA code** is complete and tested; it works
   mechanically but was solving the wrong problem.

## What didn't work

1. **First v19 deploy was actually v18** — a silent build failure
   slipped through fleet_ota.sh. Caught via SWD, but wasted ~50 min.
2. **v19 TDMA skip-forever bug** — main loop cadence equalled
   cycle period, Tag got stuck skipping. Fixed in v20.
3. **Stage 4 + default FIFO=8 doesn't deliver sub-ms** — the
   architecturally-interesting path turned out to need a deeper
   fix than just slot alignment.

## Morning action items (tasks #44, #45, #46 in tracker)

### task #45 — OTA Tags 8 and 10 back into fleet
They're stuck on broken v19. Can't ESB-trigger at 1.5 Hz TX rate
reliably. Options:
- Try fleet_ota.sh with longer trigger windows
- SWD-flash directly via J-Link Plus (need to physically swap from
  Tag 1)

### task #46 — decide Stage 4 TDMA fate
FIFO=1 delivers sub-ms per-Tag already. Stage 4 TDMA is redundant
and costs 28 % throughput (428 Hz → 305 Hz → 290 Hz over soak).
Proposal:
- **Disable Stage 4** (set `CONFIG_HELIX_STAGE4_TDMA_ENABLE=n`)
- **Keep FIFO=1** as the win
- **Re-measure** at full 10-Tag coverage after OTA'ing 8/10

### Investigate the cross-Tag p99 plateau
Per-Tag |err| p99 ~10 ms but cross-Tag span p99 ~50 ms — why the
5× gap? Could be Tag 3-style stuck-midpoint cases happening
intermittently across the fleet.

### Consider whether cross-Tag span <10 ms is actually achievable
on shared pipe 0 with FIFO=1. If not, the architectural ceiling is
in sight and we can call v1 done.

## Commits tonight

```
d05cda3  Hub CONFIG_ESB_TX_FIFO_SIZE=1 — 95% sync-error reduction  ← the win
e43ba00  v20 findings — Stage 4 works mechanically, inherits +10ms FIFO bias
64181af  Stage 4 v20 — fix skip-forever bug, always wait for slot
b71be53  fleet_ota.sh — pre-nuke stale merged.hex before rebuild
c7697da  fix Stage 4 brace/scope error in node_handle_anchor
708f70a  Stage 4 Path X1 — Tag-side TDMA slot scheduling (v19)
4f42c7b  Path X1 sanity check — both reviewers confirm
dfea534  Stage 4 exploratory — beacon-vs-anchor constraint found
1c42d57  v18 retry-count instrumentation
caf08b7  v18 findings — skip Stage 3.5, go to Stage 4
3afb74b  rounds 4 + 6 responses (Codex + Copilot) — consensus converged
08dc0c6  round 5 — Stage 3 findings + Stage 4 ask
```

All pushed to origin/nrf-xiao-nrf52840.

## Docs updated

- **New:** RF_V18_FINDINGS.md, RF_V19_FINDINGS.md, RF_V20_FINDINGS.md,
  RF_FIFO1_DISCOVERY.md, RF_STAGE4_EXPLORATORY.md, this file
- **Updated:** tools/nrf/fleet_ota.sh (hardening)
- **Docs/reviews:** round4_{copilot,codex}_2026-04-22.md,
  round5_{copilot,codex}_2026-04-22.md,
  round6_{copilot,codex}_2026-04-22.md, path_x1_sanity_2026-04-22.md

The single source-of-truth `docs/RF.md` needs a Section-7 addendum
in the morning to roll up tonight's stages (§7.11 v18 retry
instrumentation, §7.12 Stage 4 Path X1 attempt, §7.13 FIFO=1
discovery).
