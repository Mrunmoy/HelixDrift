# v18 retry-instrumentation findings (2026-04-22 night)

Per round-6 consensus: instrument retry timing first, decide Stage
3.5 based on data. **Answer: skip Stage 3.5, go straight to Stage 4.**

## Capture

- Firmware v18 (retry-instrumentation, no behaviour change).
- 20-min fleet capture, 30-s settle, 512k frames, 10 Tags healthy.
- Tag 1 SWD-read of in-RAM 2D histogram `mid_step_by_retry[4][4]`.

## Tag 1 `mid_step_by_retry` (retry bucket × mid_step bucket)

Counts of accepted anchors:

|  | mid <2 ms | 2–10 ms | 10–30 ms | ≥30 ms | row total |
|---|---:|---:|---:|---:|---:|
| Retry=1 (first try) | 287 | 98 | 1 | **0** | 386 (31 %) |
| Retry=2–3 | 95 | 56 | 3 | 1 | 155 (13 %) |
| Retry=4–6 | 103 | 109 | 0 | 3 | 216 (18 %) |
| Retry=7+ | 241 | 232 | 1 | 4 | 478 (39 %) |
| column total | 726 (59 %) | 495 (40 %) | 5 (0.4 %) | 8 (0.6 %) | 1235 |

## Reading

The reviewer-hypothesised "retries drive the ≥ 30 ms tail" is NOT
supported by the data:

- ≥ 30 ms events: **0 / 386** at retry=1, then 1 / 155 / 3 / 4 across
  higher retry buckets. Modest correlation, not a "driver" relationship.
- 10–30 ms bucket is already tiny (5 events = 0.4 %). Landing Stage
  3.5 (update ring on each retry) would cap the potential error at
  half the retry stagger — ≤3 ms per event — which can't turn 30 ms+
  into sub-ms.
- Even retry=1 anchors (no ambiguity at all) produce 98 events at
  2–10 ms and 1 event at 10–30 ms. That baseline jitter is pure RTT
  variance, not retry ambiguity.

**Therefore Stage 3.5 is not worth landing.** The residual ≥30 ms
tail (0.6 % total) is dominated by something else — likely
occasional radio glitches or TX path outliers — and would require
a different architectural fix.

## Surprise: retry count distribution

Only **31 % of accepted anchors** came from first-attempt ACKs.
**39 % needed 7+ retries.** That's a far higher retry load than
expected.

Interpretation: with 10 Tags sharing pipe 0 at 50 Hz each, air
contention is severe. A Tag frequently needs many retries before
one ACKs. That's not broken — ESB is doing its job — but it
confirms the shared-pipe architecture is collision-limited in a way
TDMA will completely resolve.

## Fleet sync quality (10 Tags, 20 min)

Nine healthy Tags:
- Mean bias range: -6.2 to -10.0 ms (spread 3.8 ms)
- Per-Tag |err| p99: 13.5 – 18.5 ms
- Cross-Tag span p50 / p99: 25.0 / 53.0 ms

Tag 10 startup artefact (mean bias pulled to -107 ms by a brief
early-boot transient with max 224 s). p99 per-frame err for Tag 10
is clean at 17.5 ms — only a handful of early samples.

Overall **stable at the same architectural limit as v17**. v18 is
retention-only change; no regression.

## Decision

Per reviewer consensus (round 6) and this data:

1. **Skip Stage 3.5.** Retry ambiguity is not the dominant source of
   the residual tail.
2. **Go straight to Stage 4 Path X1** (Tag-side TDMA scheduling on
   top of Stage 3 midpoint, no separate broadcast beacon — see
   `RF_STAGE4_EXPLORATORY.md`). This is the architectural fix that
   can actually deliver sub-ms p99.
3. **Run multi-hour v17/v18 soak in parallel** for D4 blocker
   evidence.

Proceeding.
