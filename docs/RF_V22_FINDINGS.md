# v22 glitch-reject findings — hypothesis refuted (2026-04-23)

Per docs/RF_CROSS_TAG_SPAN_INVESTIGATION.md, hypothesized that the
cross-Tag span p99 ~42 ms on v21+FIFO=1 was driven by ~1.8 % of
midpoint updates producing large single-sample jumps (bleed-through
cross-contamination). Implemented glitch-reject in v22: drop any
update where |new_midpoint - current| ≥ 10 ms, with 5-consecutive
safety valve for legitimate big shifts (Hub reboot, etc.).

## Result: hypothesis refuted

Fleet OTA to v22 deployed on 9/10 Tags (Tag 10 still stuck on v19).
15-min capture, clean 7-Tag cohort (excluding Tags 1, 3, 10):

| Metric | v21 | v22 |
|---|---:|---:|
| Mean bias range | -657..+304 µs | -1008..+677 µs |
| Per-Tag \|err\| p99 | 6.5-9 ms | 6-10.5 ms |
| Cross-Tag span p50 | 17.5 ms | 20.5 ms |
| Cross-Tag span p99 | 42.5 ms | **47.5 ms** |

Cross-Tag span p99 went UP by 5 ms, not down 35+ ms as hypothesized.

## Worse: glitch-reject broke stuck Tags

Tag 1 and Tag 3 both drifted into bad midpoint baselines and
**glitch-reject actively prevented recovery**:

- **Tag 1:** mean bias jumped to -14.6 ms (was -296 µs on v21).
  Max |err| 537 seconds — a single-frame transient event, so p99
  (7.5 ms) still looks clean, but the mean is pulled significantly.
- **Tag 3:** mean bias **2.57 seconds**. All frames for this Tag
  emitted sync_us that's 2.57 s off from Hub's rx_us.

**Root cause of the stuck Tags:**

The glitch-reject assumed "good baseline + occasional outliers." But
reality can be "bad baseline (from a bad first sample) + mix of
outliers and truly-correct updates that look like big jumps."

Example for Tag 3:
1. Boot: midpoint_offset_valid = 0. First anchor's midpoint becomes
   baseline (no glitch check because the check requires valid == 1).
2. If that first sample was an outlier (say 2.57 s off), subsequent
   correct samples look like 2.57 s "glitches" and get rejected.
3. Consecutive-reject safety valve (5 in a row) SHOULD force
   re-baseline — but in practice the stream has interleaved near-
   baseline samples (which extend the bad baseline) mixed with big-
   jump samples (which get rejected). Streak never hits 5.

So glitch-reject is fundamentally incompatible with "initial lock
may be wrong."

## What this tells us about the fat-tail root cause

Since glitch-reject didn't improve cross-Tag span p99, the ~1.8 %
of mid_step jumps ≥ 10 ms are NOT the driver of the span fat-tail.

**Revised hypothesis:** the cross-Tag span fat-tail is driven by
per-Tag mean bias DRIFT, not single-sample jumps. Each healthy
Tag has a small systematic offset (±few ms) that slowly drifts
relative to other Tags. At any given instant, one Tag's clock is
+8 ms from fleet median while another's is -10 ms, giving 18 ms
span. Over time, the drift pattern changes and different Tags are
at the extremes.

Evidence supporting this:
- v20 soak showed per-Tag |err| p50 is 500-1000 µs but cross-Tag
  span p50 is 32 ms. The gap between p50s (not p99s) is also 30×.
- Tags whose clocks drift +3 ms from fleet mean will consistently
  be the "high" Tag in a bin, not randomly spike.

A true fix would be: **hub-authoritative clock source + per-Tag
bias correction on the PC side**. Individual Tags' systematic
biases aren't going away with estimator tweaks — they come from
physical sources (crystal variation, antenna placement, ISR jitter
per board).

## Action taken

Reverting v22 changes. The glitch-reject made things neutral-to-worse
and actively prevented stuck-Tag recovery. v23 = v21 behavior +
the Stage 4 cleanup (keep that win, drop the glitch-reject).

Fleet will stay on v22 until user is back — no point OTA'ing v23
without measurement and user approval. Document the hypothesis
refutation as the value from this experiment.

## Lessons learned

1. **Initial-lock quality matters more than single-sample filtering.**
   If first midpoint is bad, no amount of downstream filtering saves
   you. Future approach should validate the initial lock (require N
   consecutive samples within tolerance before committing).
2. **Cross-Tag span fat-tail is a bias-drift problem, not an
   outlier problem.** The statistical evidence (similar p50 and p99
   gap ratios) points at systematic per-Tag offset drift, not
   sporadic spikes.
3. **Rigor on per-Tag bias correction on PC side** is the realistic
   sub-ms path. Each Tag has a semi-stable bias that can be
   measured during a calibration sweep and subtracted before
   downstream fusion.
