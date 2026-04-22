# Stage 4 design — TDMA scheduled slots for sub-ms cross-Tag span

Status: **design draft, gated on user approval**. Not yet implemented.

Parent: [RF_SYNC_DECISION_LOG.md](RF_SYNC_DECISION_LOG.md)  
Prior stage closeout: [RF_STAGE3_CLOSEOUT.md](RF_STAGE3_CLOSEOUT.md)

## Why

Stage 3.6 delivered per-Tag |err| p99 of 13.5-17.5 ms across all 10
Tags, but cross-Tag instantaneous span is still p99 = 53.5 ms. The
v1 requirement target is < 1 ms p99. Every sync tweak short of
architectural change has been exercised; remaining levers are:

1. **TDMA slots** (this doc): eliminate FIFO queuing by giving each
   Tag its own air slot. At most one Tag transmits at a time, so
   anchor delivery is deterministic and the shared ACK-payload FIFO
   never builds up.
2. **Per-Tag ESB pipes** (task #35): give each Tag its own pipe
   address so ACKs are directed. Blocked by 8-pipe hardware limit
   (our fleet is 10).

TDMA is the cleaner long-term solution but requires bootstrap sync,
which Stage 3 provides.

## Slot architecture

```
20 ms major cycle, divided into 10 × 2 ms slots, one slot per Tag:
     0    1    2    3    4    5    6    7    8    9
  |Tag1|Tag2|Tag3|Tag4|Tag5|Tag6|Tag7|Tag8|Tag9|Tag10|
```

**Slot contents (2 ms):**
- 0.0-0.3 ms: slot-start guard (Tag's ISR wakes up, computes when to TX)
- 0.3-0.4 ms: air time for 32-byte ESB frame @ 2 Mbps
- 0.4-0.6 ms: TIFS + Hub ACK TX
- 0.6-0.8 ms: retry window (Tag can retransmit if TX_FAIL)
- 0.8-2.0 ms: quiet time (no air contention)

At 50 Hz per-Tag TX rate (current target), each Tag TXes once per
20 ms cycle — fits perfectly into its own 2 ms slot.

## Slot assignment

Tag's `node_id` → slot index (1-based: node_id 1 uses slot 0,
node_id 2 uses slot 1, etc.). Slot count = `CONFIG_HELIX_MOCAP_TDMA_SLOTS`,
default 10.

## Bootstrap: Stage 3 → Stage 4 transition

TDMA requires each Tag to know Hub's clock within < 1 ms. Stage 3
midpoint delivers |err| p99 = 17 ms today — not tight enough to
land in the correct 2 ms slot. We need tighter bootstrap.

**Approach: Hub beacon + PLL-style lock.**

Hub transmits a "TDMA beacon" every major cycle (20 ms) in slot -1
(reserved 2 ms pre-slot). Beacon contains `hub_us = Hub's clock
right at beacon TX`. Tags listen in beacon slot, compute offset,
run an EMA filter for 10-20 cycles (= 200-400 ms) to converge to
sub-ms accuracy. During this bootstrap Tags TX normally (ESB mode).
Once EMA stabilises (|Δoffset| < 500 µs for 5 consecutive cycles),
Tag transitions to TDMA slot emission.

**Beacon mechanics:** Hub uses an additional pipe (pipe 1) with a
broadcast address so all Tags can RX without each having their own
pipe. Beacon is pure TX from Hub — no ACK, so it doesn't hold up
the regular ESB ACK-payload FIFO.

## Slot timing (Tag side)

On each cycle start (inferred from the EMA-corrected Hub clock):

```c
uint32_t hub_us = now_us() - stage4_offset_us;  // EMA-corrected
uint32_t cycle_start = hub_us - (hub_us % CYCLE_US);
uint32_t my_slot_start = cycle_start + (g_node_id - 1) * SLOT_US;
uint32_t delay_us = my_slot_start - hub_us + SLOT_GUARD_US;
k_sleep(K_USEC(delay_us));
esb_write_payload(&tx_payload);
```

Tag uses a high-resolution timer (NRF52 TIMER peripheral) for
slot-alignment precision. `k_sleep` jitter would be ~50-200 µs;
TIMER gives us < 10 µs.

## Hub side

Hub stays in ESB PRX mode like today. But the ACK-payload FIFO is
now guaranteed empty when a TX arrives (because the previous slot's
ACK has long since drained — 2 ms slot width vs. 130 µs TIFS). So:
- `rx_node_id` rejection ratio drops to near 0 %.
- `anchor_tx_us` is now accurate to within a few µs of the TIFS
  hardware TX time (no FIFO queue).
- Midpoint math is still the right thing, but residual error is now
  pure RTT variance (~100 µs), not queue latency.

Hub emits the beacon in slot -1 via a Zephyr work-queue item
driven by a TIMER compare match.

## Expected performance

- Slot collisions: 0 (by design)
- Anchor-delivery Tag-match rate: ~100 % (FIFO never deep)
- Per-Tag |err| p99: < 1 ms (pure RTT variance)
- Cross-Tag span p99: < 1 ms (same)

This would meet the v1 requirement.

## Failure modes and fallback

- **Beacon miss:** Tag missed N beacons in a row → EMA stale →
  transition back to Stage 3 mode (free-run ESB TX) until lock
  re-acquires.
- **Slot collision from unsynced Tag:** A Tag still in Stage 3
  bootstrap might TX in someone else's slot. Harmless (ESB
  handles collision via retry) but adds noise. Mitigation: Hub
  assigns slot only after confirming the Tag's lock quality via a
  handshake.
- **Hub reboot:** Tags lose lock; EMA re-bootstraps in ~400 ms.
  During that window, Tags fall back to Stage 3.

## Implementation staging

1. Beacon TX on Hub (Zephyr work-queue, TIMER-driven).
2. Beacon RX on Tag + EMA filter.
3. Stage 4 `stage4_offset_us` plumbed into `node_fill_frame` in
   parallel with Stage 3 `midpoint_offset_us`.
4. Measurement: compare Stage 3 vs. Stage 4 sync error on same
   fleet via an A/B split (half Tags on each).
5. Add TIMER-driven slot scheduling on Tag.
6. Full 10-Tag TDMA validation.
7. Tune slot width if < 2 ms is safe.

Every step is individually testable. Stage 3 remains available as
fallback throughout.

## Open questions for user

1. Is sub-ms cross-Tag span actually required for v1, or is the
   current 53 ms p99 acceptable for early mocap fusion work? (If
   the PC-side fusion is tolerant to ms-scale inter-node skew,
   Stage 4 is overkill.)
2. Do we want Stage 4 before Task #35 (per-Hub pipe derivation for
   multi-Hub isolation)? They're somewhat orthogonal but both
   consume pipe addresses.
3. Acceptable to burn a whole ESB pipe (out of 8) on Hub→Tag
   broadcast beacons?

**Estimated implementation effort:** 2-3 days of focused work plus
1 day of on-hardware tuning. Not a quick win — confirm the
requirement first.
