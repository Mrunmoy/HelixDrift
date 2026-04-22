# v20 Stage 4 findings — architectural limit found (2026-04-23 early)

Second Stage 4 deploy (v20) after fixing the skip-forever bug.

## Deploy

- v20 fleet OTA: 8/10 PASS, 2/10 FAIL (Tags 8 and 10 didn't upgrade
  — they were on broken v19 with <1 Hz TX rate, so the ESB OTA
  trigger couldn't reach them reliably in the 24-s timeout window).
- Tag 1 slot0 = 0x14 confirmed.
- Hub firmware unchanged (still Stage 3 anchor).

## 15-min capture (8 Tags on v20, 2 Tags stuck on v19)

| Tag | rate | mean bias | \|err\| p99 |
|---:|---:|---:|---:|
| 1 (v20) | 30.6 Hz | -10128 µs | 16.5 ms |
| 2 (v20) | 31.3 Hz | -10056 µs | 15.5 ms |
| 3 (v20) | 30.7 Hz | -10163 µs | 19.5 ms |
| 4 (v20) | 31.1 Hz | -10140 µs | 18.5 ms |
| 5 (v20) | 31.2 Hz | -10138 µs | 19.0 ms |
| 6 (v20) | 30.8 Hz | -10186 µs | 18.5 ms |
| 7 (v20) | 30.9 Hz | -10086 µs | 18.5 ms |
| 8 (v19) | 1.4 Hz | -9829 µs | 17.5 ms |
| 9 (v20) | 30.9 Hz | -10122 µs | 18.5 ms |
| 10 (v19) | 1.5 Hz | -9866 µs | 14.0 ms |

**Headline numbers:**
- Fleet mean bias: **-10071 µs** (range -10186 to -9829,
  **spread ±0.2 ms** — 9× tighter than v17's ±1.8 ms)
- Per-Tag |err| p99: 14.0 – 19.5 ms (unchanged vs v17)
- Cross-Tag span p50 / p99 / max: **33.0 / 47.5 / 57.0 ms** (similar
  to Stage 3.6's 25 / 53.5 / 65 ms)

## Tag 1 Stage 4 RAM state (post-capture)

```
stage4_state_val    = 0 (FREE) — currently unlocked
stage4_promotions   = 47
stage4_demotions    = 47       → cycles in/out of LOCKED
stage4_tx_in_slot   = 30,886   → ~10 min of LOCKED time total
stage4_tx_skipped   = 0        → skip-forever bug is gone (good)
midpoint_offset_us  = +1.69e9  (valid)
g_tx_attempts       = 102,986
g_tx_success        = 35,684   → only 34.7 % success rate
g_tx_failed         = 67,322   → 65.3 % of TXes never ACKed
```

TDMA lock mechanism works (promotions happen). Slot alignment
works (tx_in_slot accumulates). But tx_failed is huge (65 %),
meaning collisions persist despite TDMA.

## Hub ack_lat distribution (v20 window)

```
<2ms    : 2365  (0.9 %)    ← expected if TDMA worked: ~100 %
2-10ms  : 35,233  (14 %)
10-30ms : 84,661  (34 %)
>=30ms  : 132,355  (51 %)  ← FIFO still queuing!
```

With TDMA, if only one Tag TXes at a time, the ACK-payload FIFO
should drain between TXes → ~100 % of anchors in <2 ms bucket. We
see 0.9 %. So TDMA slots are NOT actually isolating Tags on-air.

## Root cause discovery — the 10 ms bias

Tag 1 SWD readout shows `midpoint_offset_us = +1.69e9`. Parsing
this vs. actual Tag/Hub uptimes gives an **effective offset bias
of +10 ms**. This bias is IDENTICAL across all 10 Tags (-10071 ±
0.2 ms mean bias in the capture data).

**Origin of the +10 ms:**

The Stage 3 midpoint estimator assumes a SYMMETRIC round trip.
Physical reality with ACK-payload FIFO:
- Tag TX → Hub RX: ~130 µs (fast — TIFS hardware)
- Hub queues anchor, waits in FIFO ~10–30 ms
- Hub ACK-TX → Tag RX: ~130 µs + FIFO queue time (slow)

Tag's midpoint `(T_tx + T_rx) / 2` sits in the middle of a round
trip that is asymmetrically weighted TOWARDS the late side. Hub's
`(central_ts + anchor_tx_us) / 2` sits near Hub RX (since
`anchor_tx_us` is queue-time not TX-time).

So `tag_mid` is ~10 ms past the physical mid-RTT (because Tag RX is
delayed by FIFO), while `hub_mid` is ~0 ms past mid-RTT (because
Hub's "TX time" is really its queue time, not radio TX time).

Net: `offset = tag_mid - hub_mid` has a built-in +10 ms bias.

## Why TDMA doesn't fix it

TDMA slots give each Tag exclusive airtime, so the FIFO SHOULD
drain. But:

1. Tags 8 and 10 are stuck on v19 → TXing at 1.4 Hz in random
   slots, keeping FIFO non-trivially populated.
2. TDMA lock jitter (±15 ms p99) is LARGER than slot width (2 ms),
   so the 8 locked Tags don't actually hit disjoint slots — they
   spread across the cycle.
3. The 10 ms bias in `midpoint_offset_us` itself means Tag's idea
   of "Hub's slot 0 start" is 10 ms off Hub's actual slot 0 start.

Confirmed by the Hub's ack_lat histogram: 51 % of ACK payloads
still take ≥30 ms to TX. FIFO is deep.

## What Stage 4 DID deliver

- Fleet-wide bias consistency improved **9×** (±1.8 ms → ±0.2 ms
  across 7 Tags). For any downstream PC fusion that subtracts the
  fleet mean bias, this is a genuine win.
- Midpoint lock mechanism works. The lock-acquire / lock-drop state
  machine correctly tracks Stage 3 stability.
- No regression in cross-Tag span (stayed at ~50 ms p99).

## What Stage 4 DID NOT deliver

- Sub-ms cross-Tag span (target: < 1 ms, still at ~50 ms).
- TX rate dropped 43 Hz → 30 Hz (due to `k_usleep` within main
  loop — each iteration now takes `20 ms + delay_us`). Cost of
  slot alignment without a proper k_timer.

## Required for sub-ms — beyond tonight's scope

To actually hit sub-ms, at least one of:

1. **Stamp `anchor_tx_us` at actual hardware TX time**, not queue
   time. Requires either:
   - NCS ESB driver modification to expose TX_SUCCESS µs timestamp
   - PPI + TIMER direct capture on RADIO.EVENTS_END (bypasses
     ESB driver timestamp — feasible but high effort)
2. **Hub broadcast beacon** on a separate ESB pipe, carrying Hub's
   clock at *radio TX start*. Tag's one-way sync via beacon is
   not subject to FIFO asymmetry. Original RF.md §9 proposed
   this — revisit the mode-switching concerns with actual
   measurement.
3. **Narrow ACK-payload FIFO to depth 1**, forcing Hub to only
   enqueue fresh anchors. Trades throughput for sync quality.
4. **Per-Tag ESB pipe addresses** (blocked by 8-pipe hardware
   limit vs. 10 Tags — if we cut fleet to 8 Tags, this works).

## Recommendation for user (wake-up note)

**Stage 4 as implemented (v20) is not the sub-ms win we hoped.**
It gave us ±0.2 ms fleet bias consistency but same 50 ms cross-Tag
p99. The 10 ms midpoint bias is an architectural limit of the
midpoint-on-FIFO approach.

Options to discuss:
- **A.** Revert the fleet to v17 (Stage 3.6 baseline), accept
  50 ms p99 as "usable for slow mocap", defer sub-ms to a future
  hardware-timestamp work.
- **B.** Leave fleet on v20, pursue one of the four sub-ms paths
  above. Estimated: A or B is ~1 day, C is ~2 hours, D is a
  hardware decision.
- **C.** Dig deeper into WHY the FIFO is still deep under v20
  TDMA. If we can force FIFO depth 1 and confirm midpoint bias
  goes to zero, that's a validation of the hypothesis and could
  turn Stage 4 into the real win.

My lean: C. It's the cheapest investigation and could confirm or
refute the 10-ms-bias hypothesis definitively.

Fleet is left on v20 overnight; a long soak is running to
accumulate stability data.
