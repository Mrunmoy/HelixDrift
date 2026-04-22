# Cross-Tag span fat-tail investigation (2026-04-23 morning)

Question: per-Tag `|err|` p99 is 5–10 ms, but cross-Tag span p99 is
~42–49 ms. Why the 5× gap?

## Approach

Wrote `tools/analysis/find_span_outliers.py` to scan the 4-h
overnight soak CSV and find every 50-ms bin where cross-Tag span
exceeds 30 ms. Report which Tag was the outlier, by how much, and
the frequency per Tag.

Command used:
```bash
python3 tools/analysis/find_span_outliers.py \
    /tmp/helix_tag_log/overnight_soak.csv \
    --threshold-ms 30 --exclude-nodes 8 10
```

## Results

**4,184 bins with span > 30 ms** out of ~85,900 bins total — **4.9 %
of bins**, which matches the p99 observation (49.5 ms).

Top 20 outlier events showed individual Tags drifting **±23 to
±32 ms** from the fleet median in each event:

```
rx_us=2951750000 span=49.5ms worst=node1 (+29.6ms)
rx_us=4088250000 span=47.3ms worst=node7 (-27.6ms)
rx_us=993700000  span=45.9ms worst=node6 (-32.3ms)
rx_us=3980300000 span=45.6ms worst=node1 (-29.1ms)
rx_us=4281350000 span=45.4ms worst=node9 (+26.6ms)
...
```

**Outlier frequency distributed roughly uniformly across all 8
healthy Tags:**

| Tag | Outlier bins |
|---:|---:|
| 5 | 680 |
| 7 | 628 |
| 2 | 594 |
| 9 | 551 |
| 6 | 507 |
| 4 | 481 |
| 1 | 470 |
| 3 | 273 |

Tag 3 has fewer because it TXes at a lower rate (still stuck on a
degraded midpoint).

## Diagnosis

**This is NOT a single bad Tag.** Every healthy Tag produces
hundreds of ±25-30 ms excursions per 4-hour window.

**Root cause hypothesis:** large single-sample midpoint updates.

Recall the `midpoint_step_bucket` distribution (Tag 1 on v20):

| Bucket | Fraction |
|---|---:|
| <2 ms | ~58 % |
| 2–10 ms | ~40 % |
| 10–30 ms | 0.4 % |
| ≥30 ms | 1.4 % |

So ~1.8 % of accepted anchors produce a mid_step jump > 10 ms.
Over a 4-hour window at ~0.9 Hz accepted rate:

```
4 h × 3600 s/h × 0.9 Hz = 12,960 updates/Tag
1.8 % × 12,960 = 233 large jumps/Tag
```

That matches the 470–680 outlier bins per Tag (each large jump
shows up in 2–3 consecutive 50-ms bins as Tag's sync_us shifts).

**The estimator accepts these large jumps unconditionally**
(Stage 3 code path in `node_handle_anchor()`). Any time the
ring-lookup succeeds, the new midpoint overwrites the previous —
even if it's 30 ms different.

## Why the large jumps happen

Likely candidates:

1. **Retry ambiguity (already measured, rejected as dominant cause).**
   v18 retry instrumentation showed retries contribute modest
   ms-scale shifts, not 30 ms class. So not this.

2. **Cross-contamination bleed-through.** With FIFO=1, most of
   the asymmetric-FIFO bias is gone, but individual anchors may
   still be built from a different Tag's frame if the Hub's
   processing ISR fires between RX of Tag A and RX of Tag B. If
   that anchor's `rx_node_id` happens to match Tag B (coincidence
   — 1/10 chance), Tag B accepts it even though the central_ts
   is from Tag A's frame.

3. **Transient radio glitches / late retries.** Occasional RF
   events where a frame's ACK is delayed by 30+ ms. The midpoint
   math then yields a biased result for that one anchor.

4. **Tag clock glitch / ISR starvation.** If Tag's main thread
   gets preempted between `tx_us` capture and `esb_write_payload`
   (or between the TX_SUCCESS event and the next frame), the
   ring's `tx_us` is stale vs. the actual radio TX. This
   corrupts the midpoint for that anchor.

Hypothesis (2) is most likely — we saw 98.6 % cross-contamination
on v13, and FIFO=1 reduces but doesn't eliminate it. The 1.8 %
residual tail is the **bleed-through rate** where a wrong-Tag
anchor happens to carry a matching `rx_node_id` byte by chance.

## Proposed fix — glitch-reject

Simplest mitigation: **reject single-sample midpoint jumps > N ms**.
Counter the rejected count for observability.

Pseudocode:
```c
if (midpoint_offset_valid && abs_delta > GLITCH_REJECT_US) {
    midpoint_glitch_rejected++;
    /* do NOT update midpoint_offset_us */
} else {
    midpoint_offset_us = new_midpoint;
    midpoint_offset_valid = 1u;
}
```

Threshold: 10 ms. (Below: normal RTT variance. Above: architectural
glitch.)

Risks:
- **Sustained real drift rejected.** If Tag's clock actually
  drifts 10+ ms in one interval (crystal failure, thermal shock),
  the glitch-reject would prevent re-lock. Mitigation: count
  consecutive rejections; after N in a row, accept the new
  baseline.
- **Initial lock.** First anchor after boot always "jumps" by a
  huge amount from the initial zero offset. Already handled —
  `midpoint_offset_valid == 0` on first sample, we ALWAYS accept.
- **Hub reboot mid-stream.** Would reset Hub clock; Tag sees
  dramatic jump. Handled the same way as initial lock — on
  repeated rejection, re-baseline after N hits.

## Expected effect

If hypothesis is right, glitch-reject should:
- Drop per-Tag `|err|` p99 from ~10 ms to **~3 ms** (just the
  2-10 ms bucket).
- Drop cross-Tag span p99 from ~42 ms to **~5 ms** (no more
  fat-tail events pulling one Tag away from fleet median).

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
