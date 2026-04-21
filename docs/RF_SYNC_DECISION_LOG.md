# RF sync decision log

**Branch:** `nrf-xiao-nrf52840`
**Opened:** 2026-04-22
**Status:** active — instrumentation in flight, wire-format rev scoped but not landed
**Inputs:**
[`docs/RF_CLOSEOUT_HANDOFF.md`](RF_CLOSEOUT_HANDOFF.md),
[`docs/RF_NEXT_STEPS_DESIGN_BRIEF.md`](RF_NEXT_STEPS_DESIGN_BRIEF.md),
[`docs/reviews/copilot_2026-04-21.md`](reviews/copilot_2026-04-21.md),
[`docs/reviews/codex_2026-04-21.md`](reviews/codex_2026-04-21.md),
[`docs/reviews/copilot_2026-04-22_followup.md`](reviews/copilot_2026-04-22_followup.md),
[`docs/reviews/codex_2026-04-22_followup.md`](reviews/codex_2026-04-22_followup.md).

This doc captures the **group-review outcome** on how to fix the ESB
sync path. It replaces the section-3.1 "three options" framing in
`RF_NEXT_STEPS_DESIGN_BRIEF.md` — both reviewers pushed back on all
three options in the brief and converged on a fourth.

## TL;DR (the decision)

1. **Reject** the originally-proposed Anchor v4 (add only
   `anchor_tx_us`) as scoped. It doesn't address the per-pipe ACK
   FIFO ambiguity that dominates the -14 ms bias today.
2. **Reject** promoting per-Tag pipes (Phase E Option A) to critical
   path for sync correctness. 8 ESB pipes don't cleanly split a
   10-Tag fleet, and even if they did, it doesn't remove the
   slow-path timing bias.
3. **Ship instrumentation first** (no wire-format change). Measure
   the actual fast-path vs slow-path fraction, anchor-age
   distribution, and offset-step distribution. This turns the wire-
   rev decision from "best-guess" into "data-driven".
4. **Ship v4 anchor with `rx_node_id` next** (14 B wire, adds 1 byte
   over current v3, same migration story as v2→v3). This is
   correctness-hardening: Tag rejects anchors that weren't built for
   it. Does not itself hit the <10 ms p99 target.
5. **Ship v5 anchor next** = `rx_node_id` + **echoed source
   frame sequence** + `anchor_tx_us`. Tag keeps a small ring of
   recent local TX timestamps keyed by sequence, pairs each received
   anchor with the correct local TX, computes
   `offset = (local_tx + local_anchor_rx)/2 − anchor_tx_us`. **This
   is the path to <10 ms p99** on shared pipe 0.

Neither reviewer recommends TDMA for this sprint. Both note it's the
scalable long-term direction if the 10-Tag-fleet ever outgrows the
shared-pipe scheme.

## Why the original three options were rejected

### Approach 1: `rx_node_id` only (shared pipe 0)

**Rejected as the full solution.** Both reviewers agreed this is the
right *first step* (ships correctness immediately, small wire bump),
but both flagged that it does NOT reach the <10 ms p99 target:

> Approach 1 fixes wrong-Tag anchors, but does not remove the ~20 ms
> slow-path estimator bias. Expect correctness to improve, but not
> <10 ms p99. (Copilot)

> It prevents bad sync bias but probably does not reach fast-motion
> <10 ms p99 by itself. (Codex)

Still worth landing as the **v4 stepping stone** between current v3
and the full v5 fix.

### Approach 2: per-Tag pipes (Phase E Option A promoted)

**Firmly rejected by both.** Same core reason from both reviewers:
8 ESB pipes can't split 10 Tags cleanly. If any pipe carries >1 Tag,
the FIFO ambiguity persists. And even if the fleet was ≤ 8 Tags,
unique pipes fix *association* but not the *slow-path timing bias*.

> For 10 Tags, this is not fully feasible on one PRX. If multiple
> Tags still share any pipe, the same FIFO-association problem
> remains. (Copilot)

> With 10 Tags and only 8 pipes, avoid "1-2 Tags per pipe" if you
> need strict per-Tag correctness. Any pipe shared by two Tags still
> has the same ACK FIFO ambiguity. (Codex)

Phase E Option A (flash-provisioned per-Hub base address) is still
useful **for its original purpose** — multi-Hub room isolation —
but is **not the sync fix**. Keeps its original priority: "only do
when multi-Hub is actually on the roadmap" (user answered: not in
the next 6 months).

### Approach 3: both (rx_node_id + per-Tag pipes)

**Rejected as over-engineered.** Inherits Approach 2's 10-vs-8
infeasibility plus Approach 1's insufficient-latency-fix. Even
Codex's endorsement of "Approach 3" was explicitly paired with "but
you also eventually need TDMA because 10 Tags don't fit on 8 pipes"
— so even Codex's yes-vote doesn't claim Approach 3 is sufficient.

## The missing option both reviewers converged on (v5 sync rev)

Both reviewers independently suggested a fuller wire rev on shared
pipe 0:

**New fields in the anchor:**

| Field | Bytes | Purpose |
|---|---:|---|
| `rx_node_id` | 1 | Which Tag's frame triggered this anchor. Tag filters: if != my node_id, ignore. |
| `rx_frame_sequence` | 1 | The sequence byte from the frame that triggered this anchor. Tag pairs this with its local ring-buffer of (sequence → local_tx_us). |
| `anchor_tx_us` | 4 | Hub's `now_us()` inside the ESB event_handler TX_SUCCESS callback, best-effort sub-ms approximation of when the anchor actually left the radio. |

**Anchor size**: v4 = 11 B (if we stage `rx_node_id` only first), v5
= 16 B (full rev). Both fit in ESB's 32-byte payload budget.

**Tag algorithm:**

```c
// On TX:
tx_ring[tx_seq & TX_RING_MASK] = local_us;

// On anchor RX:
if (anchor.rx_node_id != g_node_id) return;  // not for me, ignore
const uint32_t local_tx_us = tx_ring[anchor.rx_frame_sequence & TX_RING_MASK];
const uint32_t midpoint_local = local_tx_us + (local_anchor_rx_us - local_tx_us) / 2;
estimated_offset_us = (int32_t)(midpoint_local - anchor.anchor_tx_us);
```

Tag's clock offset is now computed from:
1. Midpoint of its own (TX time, anchor RX time) pair — correct
   Tag-local wall time at the moment Hub was physically on-air
   with the anchor.
2. Hub's reported `anchor_tx_us` — Hub-local wall time at the same
   moment.

Because RF propagation is µs-scale, these two measurements refer to
the same physical instant. Their difference is the true clock offset,
independent of:
- whether Hub hit the TIFS fast path or missed it (slow path)
- which Tag's ACK happened to carry the anchor (shared pipe)
- kernel/ISR jitter (both sides have similar jitter, roughly cancels)

Expected p99 cross-Tag span after v5 lands: **sub-10 ms**.

## Staging (execution plan)

### Stage 1 — Instrumentation (no wire change, in flight)

Goal: turn "we think ~70 % of anchors hit the slow path" into a
measured number from the fleet.

**Hub:**

- `anchor_queue_us` counter — timestamp at `esb_write_payload` call
- `anchor_tx_us` counter — timestamp inside `ESB_EVENT_TX_SUCCESS`
  handler
- Expose the TX_SUCCESS minus queue delta as a histogram in the
  SUMMARY CDC line — buckets: `<2ms`, `2-10`, `10-30`, `>30`

**Tag:**

- Record `last_tx_local_us` in `maybe_send_frame`
- On anchor RX, compute `local_rx_us - last_tx_local_us` →
  histogram buckets as above
- Compute `abs(new_offset - old_offset)` → offset-step histogram

**Deploy:** Hub SWD flash + Tag 1 SWD flash + one fleet OTA round
(~40 min per Tag × 9 Tags = ~6 h sequential, or use the fleet
harness in background).

**Measure:** 10-min baseline capture with current hardware. Commit
histograms with the data.

**Cost:** ~2-3 h firmware + deploy + measurement. No RF risk (only
counters).

**Expected outcome:** confirms or refutes the "~70 % slow path"
hypothesis with a real number. Informs whether v4/v5 is worth the
wire-format churn (vs. e.g. simply reducing the Hub's interrupt
latency so the fast path hits more often).

### Stage 2 — v4 anchor (`rx_node_id` only)

Goal: physically correct sync per-Tag (no more cross-Tag
contamination on shared pipe 0). Does NOT fix latency yet.

**Wire format v4:** 11 B (adds `rx_node_id` after `ota_target_node_id`
in the v3 layout).

Tag filter: if `anchor.rx_node_id != g_node_id` → skip sync field
update; still honour OTA_REQ flags for multi-target compatibility.

**Cost:** ~4 h firmware + one OTA round.

**Expected outcome:** `|sync_us - rx_us|` per-Tag distribution gets
tighter because noise from wrong-Tag anchors is gone. Median
unchanged, p99 tighter. Cross-Tag span p99 maybe 30-40 ms
(vs. 50 ms today).

### Stage 3 — v5 anchor (`+ frame_sequence + anchor_tx_us`)

Goal: sub-10 ms cross-Tag p99.

**Wire format v5:** 16 B (adds `rx_frame_sequence` u8 + `anchor_tx_us`
u32 after `rx_node_id`).

Tag: small TX-sequence → local-time ring buffer (4-8 entries is
plenty; anchors arrive within 1-2 frame periods). Midpoint math.

**Cost:** ~6-8 h firmware + one OTA round + measurement.

**Expected outcome:** per-Tag `|sync_us - rx_us|` median drops from
13 ms → sub-ms; cross-Tag span p99 drops from 50 ms → sub-10 ms.

### Stage 4 — (optional) TDMA / scheduled Tag slots

Only if v5 doesn't hit the p99 target (unlikely but possible under
heavy 2.4 GHz interference). Significant protocol scope. Defer
until measurement shows the need.

## Open questions the instrumentation (Stage 1) will answer

1. What's the actual fast-path / slow-path ratio today? (Stated
   ~70 % slow path is inferred from bias; direct measurement will
   confirm.)
2. Does the ratio change with Tag count? (The fleet went 9 Tags
   healthy when Tag 5 broke; a 10-Tag run might change TIFS
   collision probability.)
3. Does Hub's central_handle_frame consistently miss TIFS because
   of the work it does (frame processing, anchor build, track
   update), or is it variable based on system load?
4. Is there a per-Tag bias pattern (e.g. Tags 1-3 always fast path,
   Tags 8-10 always slow path) driven by relative TX timing?
5. Do anchors ever get silently overwritten in the ACK FIFO (i.e.
   Hub queues an anchor for Tag A, then before TX, queues another
   for Tag B, clobbering the first)?

## Risk register

- **Overfitting to instrumentation.** Stage-1 histograms are noise-
  limited and could mislead v5 design. Mitigation: run baseline
  twice, 10 min apart, see if the distributions are stable.
- **v5 firmware complexity.** A TX-sequence ring buffer on Tag adds
  ~32 bytes of state and logic in `maybe_send_frame` / anchor
  handler. Low risk, mitigated by unit-testable pure functions.
- **Slow-path on v5 still delivers late anchors.** Even with
  midpoint math, late anchors still have to arrive within a Tag's
  ring window. If a Tag is silent for > N frames, the next
  anchor's sequence won't be in the ring. Tag must fall back to
  "drop this anchor" rather than guess. Acceptable (sync catches
  up on the next fast-path anchor).
- **Tag 5 hardware-flatlined** (separate issue, tracked in
  `RF_CLOSEOUT_HANDOFF.md` §2.3). All the stage 1/2/3 measurements
  are 9-Tag-fleet until that's repaired.

## Changelog

- 2026-04-22 07:35 AEST: doc created, reflecting round-2 group
  consensus. Stage 1 firmware work kicking off.
- 2026-04-22 07:50 AEST: Stage 1 firmware landed (commit `5f02792`).
  First Hub capture showed `ack_lat=8619/13005/0/0`. Initially looked
  like good news (40 % fast path, 60 % medium-slow, no true 20 ms
  slow path). Round-3 review caught a measurement bias: the
  single-slot `last_anchor_queue_us` is overwritten on every RX, so
  the TX_SUCCESS for an older queued anchor is measured against the
  newest queue time → either dropped by the "tx_done ≥ queue" guard
  or compressed into small buckets. Exactly the slow-path cases we
  care about are preferentially hidden. Conclusion: don't trust the
  empty >10 ms buckets.
- 2026-04-22 08:10 AEST: Stage 1' ring-buffer fix landed (commit
  `0d9708a`). Ring of 16 queue timestamps, pushed on enqueue in
  central_handle_frame, popped on TX_SUCCESS. Single-ISR context in
  NCS v3.2.4, so no locking. Two new diagnostics in SUMMARY:
  `pend_max` (peak FIFO depth) and `ack_drop` (TX_SUCCESS with empty
  ring — should stay 0).
- 2026-04-22 (in progress): v13 OTA round in flight to deploy the
  Tag-side Stage-1 instrumentation to all 10 Tags. Hub Stage-1'
  ring-buffer fix pending SWD re-flash after OTA round completes.
  Then rerun a 10-min capture to get unbiased distribution before
  deciding Stage 2 implementation detail.

## Stage 1 / Stage 1' findings

### Hub ack_lat (Stage 1' ring-buffer, live 60 s sample)

| Bucket | Count | % |
|---|---:|---:|
| <2 ms (fast path) | 470 | **2.1 %** |
| 2-10 ms | 6022 | 26.5 % |
| 10-30 ms | 14362 | **63.1 %** |
| >=30 ms | 1894 | **8.3 %** |

Ring showed `pend_max = 88620` — far beyond the 16-slot ring capacity
and incompatible with a clean 1-TX_SUCCESS-per-1-RX model. Investigation
suggests **Hub RX count ≠ TX_SUCCESS count**: ACKs without a queued
payload (or with the driver-provided "empty" fallback) don't fire
TX_SUCCESS, so the consumer under-runs. Observed: 316321 anchors
queued, ~227k TX_SUCCESS events over 10 min (~72 % ratio).

So the absolute bucket magnitudes are distorted by the overflow. The
**distribution shape is still directionally useful**: <2 % fast path,
>70 % landing ≥ 10 ms late. Strongly supports the slow-path-dominance
hypothesis and justifies the v5 midpoint-estimator path.

### Tag 1 in-RAM histograms (SWD read, node_id=1)

anchor_age (Tag's local time at anchor RX minus its own most recent TX):

| Bucket | Count | % |
|---|---:|---:|
| <2 ms | 45172 | **76 %** |
| 2-10 ms | 14265 | 24 % |
| 10-30 ms | 5 | 0.0 % |
| >=30 ms | 0 | 0 % |

offset_step (|Δestimated_offset| on each anchor):

| Bucket | Count | % |
|---|---:|---:|
| <2 ms | 40383 | **68 %** |
| 2-10 ms | 18591 | 31 % |
| 10-30 ms | 477 | 0.8 % |
| >=30 ms | 11 | 0.02 % |

**anchor_age on Tag is NOT a staleness measure.** It measures Tag-TX
to Tag-ACK-RX round trip, which ESB hardware keeps fast (~1 ms)
regardless of what anchor content got attached. So 76 % < 2 ms is
just "ACK reaches Tag quickly" — an ESB property, not a sync
property.

**offset_step is the usable Tag-side signal.** The ~490 offset jumps
≥ 10 ms (including 11 ≥ 30 ms) across 59 k anchors is direct
evidence that cross-contaminated anchors are corrupting the
estimator. This is what Stage 2 (`rx_node_id`) will filter out.

### What this means for the roadmap

Hub-side signal (biased) points to heavy slow-path anchor delivery;
Tag-side signal confirms cross-contamination on the estimator. **Both
findings point to the same v5 fix** (anchor_tx_us for proper
timestamping + rx_frame_sequence for Tag-local TX correlation +
rx_node_id for association).

Staging unchanged: Stage 2 = rx_node_id first (cheap, correctness);
Stage 3 = full v5 (sub-ms sync target).

## Stage 2 implementation (v4 anchor)

**Change summary:** `HelixSyncAnchor` grows 10 → 11 bytes. The new
byte carries `rx_node_id` — the `node_id` of the Tag whose frame
produced this anchor. Tags drop the sync-estimator update whenever
`rx_node_id != own node_id`, keeping OTA-flag processing unaffected.
A new Tag-side counter `anchors_wrong_rx` is emitted in SUMMARY so
we can measure the rejection ratio directly.

**Backward compat:** size gates in `node_handle_anchor()` preserve
v1/v2/v3 semantics. A v4 Tag on a v3 Hub keeps today's "accept all
anchors" behaviour until the Hub is also on v4.

**Expected effect on metrics:**
- `offset_step` distribution on each Tag should collapse toward
  bucket 0 (cross-contaminated anchors no longer feed the estimator).
- `anchors_wrong_rx / (anchors + wrong_rx)` should approximate the
  Hub's slow-path ratio — roughly 70-90 % based on Stage 1' Hub
  histogram (most anchors landing on a later frame's ACK belong to a
  different Tag).
- `anchors_received` drops sharply — this is expected and correct:
  we now *only* update on anchors truly built from our own frame.
- Cross-Tag span p99 not expected to change dramatically — the
  slow-path latency bias is still there. Stage 3 v5 fixes that.

**Deployment plan:**
1. SWD-flash Hub (single step, no OTA).
2. OTA fleet to next version (fleet_ota.sh) — 10 Tags, ~40 min.
3. 10-min SUMMARY capture, compare offset_step histograms.
4. Compute per-Tag rejection ratio, verify ~70-90 %.

## Stage 2 findings (v4 anchor, v14 firmware, 2026-04-22)

Fleet OTA to v14 completed (10/10 PASS). 10-min capture, Tag 1 RAM
histograms SWD-read:

| Counter | Value |
|---|---:|
| anchors_received (v4-accepted, matched own node_id) | 494 |
| anchors_wrong_rx (v4-rejected, built for another Tag) | 34,165 |
| Total anchors delivered to Tag 1 | 34,659 |
| **Rejection ratio** | **98.6 %** |

**Reading:** far higher than the 70-90 % we predicted. Interpretation
of what "rx_node_id" actually measures was subtly wrong. See below.

**Revised mental model of ESB ACK-payload FIFO semantics.**
In NCS's Enhanced ShockBurst, each Tag's TX causes the Hub's
`central_handle_frame()` to call `esb_write_payload()` with a new
anchor. Those anchors queue in a per-pipe FIFO inside the ESB
driver. When the **next** frame arrives from any Tag on pipe 0
(shared), the hardware returns the OLDEST queued ACK payload to
whichever Tag just TXed.

So `rx_node_id` (set to `frame->node_id` at queue time) labels
"which Tag caused this anchor to be built," NOT "which Tag will
receive it." Those coincide only when the FIFO depth is 1 at the
moment your next frame arrives — which, with 10 Tags sharing pipe 0
and FIFO depth up to 3, is rare.

**Implications:**
1. Stage 2's correctness property still holds — when `rx_node_id`
   does match own node_id, we *know* the anchor genuinely traces to
   our own RX time. No cross-contamination on those updates.
2. Effective anchor-update rate dropped from ~60 Hz to ~0.8 Hz per
   Tag. Sync estimator now ticks ~75× less often.
3. Even when accepted, Tag's offset_step distribution stays wide:
   53.2 % / 34.0 % / 11.1 % / 1.6 % across the 4 buckets. The
   Hub-side ACK-TX latency bias (Stage 1' showed >70 % of ACK TXs
   land 10-30 ms after Tag's frame) is still in play — we're seeing
   queue latency as "sync error" on every accepted anchor.

**Conclusion:** Stage 2 confirmed cross-contamination is near-total
and is not the only bias — even correct-Tag anchors carry
significant ACK-TX-latency bias. Stage 3 v5 (midpoint estimator
with `rx_frame_sequence` and `anchor_tx_us`) is now urgent: it
fixes BOTH the cross-contamination problem (via rx_frame_sequence
ring lookup) AND the ACK-TX-latency problem (via anchor_tx_us
direct TX-side timestamp). Stage 2 should stay in place as a
correctness sanity check; Stage 3 builds on top.

## Requirements reality check

`docs/rf-sync-requirements.md` (draft v0.1, 2026-03-29) lists
**"< 1 ms inter-node skew for multi-node kinematic chains"** as the
explicit sync target for v1. Measured today (at v12 Phase C, Stage 1
instrumentation, 9 healthy Tags, bin-based metric):

- Cross-Tag span p50: **19 ms**
- Cross-Tag span p99: **50 ms**
- Per-Tag mean bias: **-14 ms** (-13.4 to -14.5 ms range)

Gap to the requirement: **~20×** at p50, **~50×** at p99. The v5 anchor
midpoint estimator fix is the biggest expected lever (removes the
systematic bias, likely collapses p99 toward ms-scale). But < 1 ms
may still be unachievable on a shared-pipe ESB design without some
combination of:
- per-Tag pipes (blocked by 8-pipe vs 10-Tag constraint)
- TDMA scheduled slots
- hardware-level TX/RX timestamping on both sides (bypass NCS ESB
  driver's event-queue ISR latency)

Recommendation: treat `< 1 ms` as an aspirational target worth
re-evaluating after Stage 3 measurements. Body-mocap applications
often work acceptably at 5-10 ms inter-node skew; hard-real-time
sports biomechanics may need the lower number.
