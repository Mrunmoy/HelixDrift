# Drift hypothesis also refuted (2026-04-23)

After v22's glitch-reject failure refuted the jump-hypothesis, the
round-10 reviewer consensus (Codex + Copilot × 2) proposed the
drift-hypothesis and a specific offline replay to validate it.

## What the experiment ran

Per Codex round-10 Q5 + Copilot "next experiment", ran
`tools/analysis/offline_bias_replay.py` on a 200k-row subset of
the overnight soak (v20 fleet + Hub FIFO=1, 7 healthy Tags):

- **Raw:** no correction (baseline).
- **Fixed bias:** subtract each Tag's overall mean bias (sync_us
  - rx_us averaged over the whole capture).
- **Rolling-10s bias:** per-Tag rolling-10-second median error
  RELATIVE to the fleet median at the same moment.

If the drift hypothesis were right, the rolling correction should
have collapsed span p99 close to the per-Tag p99 (~5 ms).

## What we got

| Strategy | Cross-Tag span p50 | p90 | p99 | max |
|---|---:|---:|---:|---:|
| Raw | 24.0 ms | 33.0 ms | **42.5 ms** | 12326 ms |
| Fixed bias | 23.4 ms | 31.7 ms | **41.5 ms** | 12324 ms |
| Rolling-10s bias | 22.5 ms | 31.5 ms | **41.0 ms** | 12326 ms |

**Only 1.5 ms improvement at p99.** Marginal. Neither correction
mode meaningfully changes the fat-tail behaviour.

Per-Tag mean bias after corrections:
- Raw: ranges -6.04 to +1.01 ms (drift spread ~7 ms)
- Fixed: ranges -0.00 to +0.00 ms (removed, as expected)
- Rolling: ranges -1.55 to -0.32 ms (small residual)

So the PC-side correction IS removing the mean-bias spread, yet
span p99 barely moves.

## So what's actually driving the fat-tail?

Both the JUMP hypothesis (v22 test) and the DRIFT hypothesis
(offline replay) are refuted. The fat-tail is something else.

**New working hypothesis** (to be validated): the fat-tail is
**per-Tag single-Tag tail events** — each individual Tag's `|err|`
distribution extends beyond p99 up to tens of ms due to rare
per-frame-level issues (retry stacking, radio glitches, ISR
jitter). Not inter-Tag bias drift.

Evidence for this:
- Per-Tag |err| p99 is 4-11.5 ms, but per-Tag |err| MAX is 10s to
  thousands of ms (Tag 1 max was 84 s in one capture, Tag 6 had
  24.6 s).
- In any given 50 ms bin, if ONE Tag hits its own tail, span
  explodes even if all others are clean.
- With N Tags × P(tail event) = 7 × 0.01 ≈ 7 %, many bins have
  at least one Tag in its tail. Matches the observed 5 % span > 30 ms.

This interpretation means the fat-tail is **irreducible on a
per-Tag basis** without fixing the underlying per-Tag tail events.
Those come from physical sources (RF interference, retry stacking,
ISR jitter) that firmware sync estimators can't fully suppress.

## What this says about the path forward

**Sub-ms cross-Tag span is likely not achievable on shared-pipe ESB
without fundamentally changing the radio architecture.** Options:

1. **Hardware TX timestamping** (PPI + TIMER capture on
   RADIO.EVENTS_END). Removes ISR jitter as a per-frame
   uncertainty source. Effort: 3-5 days.
2. **Per-Tag ESB pipes** (limited to 8 Tags per Hub). Removes
   shared-pipe ACK-payload ambiguity entirely. Requires fleet
   downsize or multi-Hub (currently blocked).
3. **TDMA with hardware-authoritative Hub beacon.** Requires (1)
   + (2) essentially.
4. **Ship with current limits.** Document that shared-pipe ESB
   gives ~40-50 ms cross-Tag span p99 with ~5-10 ms per-Tag
   p99. Suitable for mocap at up to ~30 Hz effective frame rate
   per Tag. Fast-motion use cases need one of the above.

**Firmware options short of architecture change:**

5. **Initial-lock qualification** (reviewer round-10 unanimous
   recommendation). Require N=5 consecutive samples within ±3 ms
   before committing the midpoint baseline. Prevents bad initial
   locks (would have saved Tag 3 on v22). Doesn't improve fat-tail
   but closes a stuck-state failure mode.
6. **Lock-health telemetry.** Expose acquisition age, candidate
   spread, distance-from-fleet-median as SUMMARY fields. Lets PC
   layer detect and ignore stuck-Tag state.

## Recommendation

Not implementing any of 1-6 autonomously. The user should choose:
- Declare current state (FIFO=1 + v21 behaviour) as "v1 RF done"
  with documented ~42 ms cross-Tag span p99 as acceptable.
- OR invest in architecture change (options 1-3).
- OR invest in firmware hygiene (options 5-6) which don't
  directly improve span but make the system more diagnosable.

The autonomous-sprint value add is in documentation and
experiments that narrowed the problem space — not in continuing
to pick speculative fixes without a clear architectural signal.

## Tool committed

`tools/analysis/offline_bias_replay.py` stays in the repo for
future use. Run it against any future capture to re-test these
hypotheses as data accumulates.
