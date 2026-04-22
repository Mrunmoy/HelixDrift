# Stage 4 exploratory notes (2026-04-22, night work)

Parent: `docs/RF.md` §9 Stage 4 TDMA proposal.

These notes are the night-work design exploration while waiting on
v18 fleet OTA. They should feed back into `docs/RF.md` once
consensus is reached.

## Beacon architecture — constraint we missed

Stage 4 §9.3 proposes a "Hub beacon every 20 ms in a reserved
pre-slot, pipe 1, broadcast address". Implementing this cleanly
runs into an ESB hardware constraint:

**ESB hardware is PRX XOR PTX per instance.** An NCS ESB instance
configured with `ESB_MODE_PRX` cannot also do arbitrary TX. Switching
modes requires `esb_disable` → `esb_init(PTX)` → TX → `esb_disable` →
`esb_init(PRX)`, which:
  - takes ~1-2 ms per cycle
  - empties the ACK-payload FIFO (so any queued anchors lose their
    TX slot)
  - risks a race where a Tag TXes during the PTX→PRX transition and
    gets no ACK (appears as TX_FAILED on the Tag)

Three possible paths:

### Path X1 — Use existing anchors as beacons (no new protocol)

Observation: every anchor **already** carries `anchor_tx_us` = Hub's
clock at TX. That's literally a beacon. The only difference between
an anchor and a beacon is the `rx_node_id` filter — anchors are
per-Tag (98.6 % rejection on shared pipe 0); a beacon would be
unicast-to-all.

**Proposal X1:** keep the existing anchor protocol. For Stage 4
TDMA bootstrap, each Tag treats:
- Anchors with `rx_node_id == own node_id` as **high-confidence**
  (midpoint estimator input, like today).
- Anchors with `rx_node_id != own node_id` as **low-confidence
  bootstrap-only** (coarse offset snapshot, used only when Tag has
  no valid midpoint yet).

This is what Copilot round-5 called "bootstrap-only coarse mode".
It needs no wire format change, no Hub mode switching, and no new
ESB pipe.

**Cost:** ~1 hour firmware. Pure Tag-side change.

### Path X2 — Mode-switching Hub beacon (the original proposal)

Hub runs in PRX most of the time, briefly switches to PTX every
20 ms to emit a broadcast beacon on pipe 1, then switches back.
As designed in RF.md §9.3.

**Problems** (discovered this pass):
1. Mode-switch latency ~2 ms → during that window, Tags TXing to
   Hub see TX_FAILED. Over a 20 ms cycle that's 10 % dropped frames
   — unacceptable.
2. ACK-payload FIFO is wiped on `esb_disable` → every mode cycle
   loses queued anchors. Stage 2 accept rate (already 0.9 Hz) would
   drop further.
3. Requires reworking the ESB driver init/reinit flow in the Hub.

**Cost:** 2-3 days firmware + fragile per-boot mode-switch timing.

### Path X3 — Radio-level bypass (PPI + nrfx_radio)

Use the nRF radio peripheral directly, bypassing the NCS ESB
driver for beacon TX only. The ESB driver owns TIMER0 and RADIO
via MPSL — wiring a PPI channel from a separate TIMER to RADIO for
beacon TX would require careful coexistence with MPSL.

**Cost:** 4-5 days firmware + high risk of MPSL/ESB interference
that invalidates all prior Stage 2/3 measurements.

**Not worth it for a v1 sync fix.**

### Recommendation

**Take Path X1.** Existing anchors are already beacons; the
"separate beacon packet" concept in §9.3 was conflating two
things. For Stage 4 TDMA to work, Tag just needs a reliable
time-to-Hub-clock — Stage 3 midpoint delivers that today when
the anchor matches. For anchors that don't match (98.6 %), we
can optionally use them at low confidence for bootstrap.

This lets overnight work focus on the Tag-side TDMA scheduling
(where all the sub-ms risk is) instead of fighting ESB driver
internals.

## Revised Stage 4 architecture (Path X1 variant)

```
Existing (Stage 3.6):                   Proposed (Stage 4):

Tag TXes anytime (50 Hz).               Tag TXes ONLY in its slot.
Hub ACKs as FIFO allows.                Hub ACKs, FIFO stays empty
Anchor carries central_ts +             (one Tag in flight at a time).
  anchor_tx_us.                         Anchor midpoint becomes sub-ms.
Tag runs midpoint on matched            Tag runs midpoint on matched
  anchors (1.4 % hit rate).             anchors (~100 % hit rate).
```

The only **Tag-side** change: sleep until your TDMA slot, then TX.

The only **Hub-side** change: maybe a tighter `esb_write_payload`
ISR path to cut the 130 µs TIFS window (nice-to-have, not needed
for sub-ms).

## Overnight plan revision

Based on Path X1 being much cheaper than the original Stage 4
proposal, order becomes:

1. **v18 retry-instrumentation** — already OTA in flight. Answers
   Stage 3.5 question. [~40 min remaining]
2. **v18 capture + analysis** — decide if Stage 3.5 is worth it.
   [~20 min]
3. **Stage 4 TDMA (Path X1 — no Hub changes yet)** — add TIMER-
   driven slot scheduling on Tag. Needs:
     - Slot width/cycle from Kconfig (default 2 ms slots × 10 =
       20 ms cycle).
     - Tag's midpoint_offset_us → Hub-time conversion inside a
       TIMER ISR.
     - Slot-aligned `esb_write_payload` instead of free-run
       `maybe_send_frame`.
     - `stage4_lock_pending` gate: stay free-running until midpoint
       has < 500 µs jitter for N consecutive updates.
   [~3-4 hours firmware + 1 fleet OTA = 5-6 hours total]
4. **Multi-hour v17 soak in parallel** — while Stage 4 code is
   being written. Zero firmware cost, just a long capture.
5. **A/B Stage 3.6 vs Stage 4** on same fleet. Confirms sub-ms.

Total overnight realistic: steps 1-3 done + soak collected by
morning.

## Risks & fallbacks

**Slot misalignment at boot:** Tag boots, doesn't have a midpoint
lock yet. Solution: start in Stage 3.6 free-running mode. Only
transition to TDMA after midpoint has been valid and stable for
5+ consecutive updates (100+ ms worth). Fall back to Stage 3.6
if midpoint lock is lost (e.g., ≥ 3 consecutive `seq_lookup_miss`).

**Clock drift between slot decisions:** Tag schedules its next TX
based on its current `midpoint_offset_us`. If midpoint updates
come in while the TIMER is armed, the slot time shifts. Solution:
use a hysteresis band — only re-arm TIMER if midpoint moved by > N
µs since the last arm. Keeps the slot plan stable.

**Hub TIFS still sometimes missed:** even with only one Tag in
flight, if the Hub's `central_handle_frame` takes > 150 µs, ACK TX
still misses TIFS → anchor gets queued → delivered next cycle,
which is 20 ms later. This reintroduces the ~20 ms tail on
matched anchors. Solution: time-profile `central_handle_frame`,
confirm it's < 100 µs typical. If not, trim it.

**ESB ISR jitter:** TIMER ISR latency on nRF52 at 64 MHz with our
priorities is ~5-30 µs typical, 100 µs worst case. Slot guards
of 0.3 ms should cover this. If not, move to HFCLK-precise TIMER
compares via PPI directly to RADIO tasks.

## Decision point

After v18 capture, I'll decide:
- **If retries dominate >=30 ms bucket:** land Stage 3.5 first,
  narrow the tail, then Stage 4.
- **If retries are uncorrelated with >=30 ms bucket:** skip Stage
  3.5, go straight to Stage 4 Path X1.

Either way, overnight delivers measurable progress.
