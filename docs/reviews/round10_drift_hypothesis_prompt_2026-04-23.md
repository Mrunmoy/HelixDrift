Round 10 — v22 glitch-reject experiment findings. Please read
docs/RF_V22_FINDINGS.md first (~2 min).

Summary: I implemented glitch-reject per the round-9 post-sprint
plan (reject single-sample midpoint jumps ≥ 10 ms). Hypothesis
was: fat-tail cross-Tag span p99 ~42 ms is driven by ~1.8 %
bleed-through cross-contamination producing single-sample bad
midpoints.

Result refuted the hypothesis:
  Cross-Tag span p99:  42.5 ms (v21)  →  47.5 ms (v22)   ← worse
  Per-Tag |err| p99:    6.5-9 ms      →  6-10.5 ms        ← wash
  Tags 1, 3:           recovered on v21  →  STUCK on v22  ← regression

Tag 1 locked onto -14.6 ms bad baseline, Tag 3 locked onto a
+2.57-SECOND bad baseline. Glitch-reject then prevented correct
updates from taking effect (they looked like "big jumps" from the
stuck baseline). The 5-consecutive safety valve didn't fire
because the stream had interleaved near-baseline samples that
kept resetting the streak.

I've reverted the glitch-reject code (v23 source = v21 behaviour +
Stage 4 cleanup). Fleet stays on v22 for now.

Revised hypothesis: the fat-tail is DRIFT-driven, not JUMP-driven.
Each Tag has a small systematic bias (crystal variance, antenna
placement, board-specific ISR jitter) that slowly drifts relative
to the fleet. At any instant one Tag is +8ms and another -10ms
from fleet median, giving 18ms+ span. Single-sample filtering
can't touch that.

Questions for your team:

Q1. Do you agree with the drift-hypothesis reading of the data?
    Per-Tag |err| p99 is ~10ms (individually clean) but cross-Tag
    span p99 is ~42ms (fleet-level spread) — same ratio as p50
    (~500us individual vs ~30ms cross-Tag). The ratio-preserved
    scaling feels like a bias-drift signature.

Q2. If yes, the remaining sub-ms path goes through the PC side:
    each Tag gets a measured systematic bias, subtracted at
    fusion time. Is this the right architectural call, or is
    there a firmware-only lever I'm missing?

Q3. What would a proper **initial lock quality check** look like?
    The v22 glitch-reject failure showed that a bad first
    midpoint sample locks a Tag into a permanent wrong baseline.
    I'd like future firmware to require N consecutive samples
    within tolerance before committing the initial lock, same
    way Stage 4 LOCK_N did for TDMA transitions. Is that the
    right shape of fix, and if so, what N / tolerance?

Q4. Is there any value in an EMA (exponentially weighted moving
    average) on the midpoint? With alpha ~0.3, single-sample
    outliers contribute only 30 % per update, so 5 bad samples
    would contribute ~83 % total — still corrupts the baseline
    over time if outliers are sustained. Might hurt more than
    help. Thoughts?

Q5. Anything else surprising in the v22 finding that I missed?

Under 500 words per reviewer. If you think this calls for a
specific next experiment, propose one.
