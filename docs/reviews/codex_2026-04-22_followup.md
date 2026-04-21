  2. PC-side fusion NOT started yet — just Hub CDC ingest — so wire-format
     changes have zero downstream cost, we're free to rework.
  3. Two-Hub same-room scenario NOT expected in next 6 months.
  4. NCS v3.2.4 ESB driver: struct esb_evt on TX_SUCCESS has only
     {evt_id, tx_attempts} — no hardware TX timestamp. Software
     timestamp via now_us() inside event_handler callback gives
     sub-ms jitter (ISR dispatch). Good enough for ms-level bias fix.
  5. Tag 5 = HTag-0126 node_id=5 physically identified, user will
     inspect hardware separately.
  6. User wants an "RF closeout sprint" now — comprehensive handoff doc
     + whichever cheap wins we can land safely.

Given these answers, please debate and come back with a SPECIFIC
recommendation on one question:

  How should we fix the sync-anchor ↔ Tag association so per-Tag
  sync is physically correct, given ACK-per-pipe-FIFO semantics?

Three candidate approaches, please rank + give rationale:

  Approach 1: "rx_node_id in anchor"
    Hub adds `rx_node_id` field to the anchor (which Tag this anchor
    was built FOR). Tags filter: if anchor.rx_node_id != my node_id,
    ignore the sync fields (still obey OTA bits). Stay on shared
    pipe 0. Wire format: v4 anchor gains 1 byte.

  Approach 2: "per-Tag pipes" (i.e., Phase E Option A promoted to
    critical path)
    Split 10 Tags across multiple ESB pipes (max 8 pipes available, so
    e.g. pipes 0..7 carrying 1-2 Tags each). Each pipe's ACK FIFO is
    independent → anchor generated for Tag A at pipe N stays at pipe
    N and is only ever delivered to Tags on pipe N. Then Anchor v4
    midpoint math works as originally scoped. Wire format unchanged.

  Approach 3: "both" — per-Tag pipes for the correctness, rx_node_id
    in anchor as a belt-and-braces sanity check Tags can use during
    roll-out.

Additional constraints the group should weigh:
  - Tag migration cost: whichever we pick, fleet currently on v12
    (10-byte v3 anchor). A wire-format bump needs one OTA round to
    deploy (well-understood at 100% reliability). Per-Tag-pipe change
    needs BOTH Hub AND Tag firmware change (same OTA cost — every Tag
    also needs its pipe ID baked in OR flash-provisioned).
  - p99 target: user wants fast-motion capture usable. <10 ms cross-
    Tag p99 span would be the goal. How close does each approach get?
  - Complexity / risk of regressions at 10-Tag fleet: we have a well-
    understood 436 Hz / +16% Phase-C baseline to protect.

Also please consider and comment:

  (A) Any third option we're missing? e.g. hub adds a TDMA-style
      slot grant, or Hub fills in `anchor.central_timestamp_us`
      only when it can attach to the intended Tag's ACK (fast path)
      and leaves it zero otherwise so Tag knows to skip that sample.

  (B) What's the minimum instrumentation change we could land FIRST
      (no wire-format change yet) that would make the decision
      data-driven? e.g. a counter of fast-path vs slow-path anchors.

Under 600 words total. Get to a call. Don't hedge too much.
codex
I’ll treat this as an architecture recommendation, not a code change. I’ll inspect the repo only if needed for names, but the core answer can be decided from the ESB FIFO semantics and your constraints.
codex
**Call: pick Approach 3, but be clear what each part solves.**

Rank:

1. **Approach 3: per-Tag pipes + `rx_node_id` in anchor**
   This is the best closeout-sprint recommendation. Per-pipe ACK FIFO isolation is the real physical fix; `rx_node_id` is a cheap invariant check that prevents silent bad sync during rollout, misprovisioning, or any remaining shared-pipe grouping.

   With 10 Tags and only 8 pipes, avoid “1-2 Tags per pipe” if you need strict per-Tag correctness. Any pipe shared by two Tags still has the same ACK FIFO ambiguity between those two Tags. The correct version is: dedicate pipes to the highest-priority active Tags, or use per-slot/rotating pipe assignment so only one Tag is eligible per pipe at a time. If 10 concurrent Tags must all be correct, pipes alone are insufficient unless combined with scheduling.

   Expected p99: best path toward `<10 ms`, but only if pipe sharing is eliminated or scheduled. `rx_node_id` makes bad anchors ignored instead of corrupting clocks.

2. **Approach 1: `rx_node_id` in anchor, stay shared pipe 0**
   This is the cheapest safe wire-format fix and should be landed even if Approach 3 follows. It makes sync association logically correct: a Tag will never apply another Tag’s anchor. But it does not make useful anchors reliably available, because the Hub still often misses TIFS and the next Tag may receive and discard the stale anchor.

   Expected p99: correctness improves, availability likely gets worse or visibly sparse. It prevents bad sync bias but probably does not reach fast-motion `<10 ms` p99 by itself.

3. **Approach 2: per-Tag pipes only**
   Incomplete as stated. ESB has max 8 pipes and the fleet has 10 Tags. If any pipe carries two Tags unscheduled, ambiguity remains. Also, without `rx_node_id`, bad provisioning or rollout mismatch can silently reintroduce wrong anchors.

   Expected p99: good only for truly one-Tag-per-pipe operation; otherwise not a full fix.

**Third Options**

TDMA/slot grants are the scalable correctness answer if 10 concurrent Tags matter. The Hub grants a Tag-specific uplink slot; the ACK payload for that slot is only meaningful for that Tag. This works even on shared pipe 0, because temporal ownership replaces pipe ownership. It is more invasive than `rx_node_id`, but it scales beyond 8 Tags and protects p99 better than best-effort ACK timing.

“Only fill `central_timestamp_us` on intended fast-path ACK, else zero” is a good safety mechanism, but not enough alone unless the Hub can prove intended delivery. On shared pipe 0 without Tag-specific ACK ownership, it mostly becomes an anchor-validity gate.

**Minimum Instrumentation First**

Land counters before the wire bump:

- Hub: `anchor_fast_path_written_before_ack_deadline`
- Hub: `anchor_slow_path_missed_tifs`
- Hub: `ack_payload_overwrite_or_pending_depth_by_pipe`
- Tag: `anchors_received`, `anchors_applied`, `anchors_rejected_wrong_node` after v4
- Tag/Hub: per-node anchor age and span histogram

Before v4, add Hub-side logging/counters estimating whether the ACK payload was queued inside the intended Tag’s response window versus after it. That gives the data needed without changing OTA format.

**Recommendation**

Ship **v4 with `rx_node_id` now**, plus instrumentation. Treat it as the correctness guardrail. Then implement **scheduled per-Tag ownership**: either true one-Tag-per-pipe where possible, or TDMA slotting for all 10 Tags. Do not rely on unscheduled shared pipes for sync.
tokens used
25,737
**Call: pick Approach 3, but be clear what each part solves.**

Rank:

1. **Approach 3: per-Tag pipes + `rx_node_id` in anchor**
   This is the best closeout-sprint recommendation. Per-pipe ACK FIFO isolation is the real physical fix; `rx_node_id` is a cheap invariant check that prevents silent bad sync during rollout, misprovisioning, or any remaining shared-pipe grouping.

   With 10 Tags and only 8 pipes, avoid “1-2 Tags per pipe” if you need strict per-Tag correctness. Any pipe shared by two Tags still has the same ACK FIFO ambiguity between those two Tags. The correct version is: dedicate pipes to the highest-priority active Tags, or use per-slot/rotating pipe assignment so only one Tag is eligible per pipe at a time. If 10 concurrent Tags must all be correct, pipes alone are insufficient unless combined with scheduling.

   Expected p99: best path toward `<10 ms`, but only if pipe sharing is eliminated or scheduled. `rx_node_id` makes bad anchors ignored instead of corrupting clocks.

2. **Approach 1: `rx_node_id` in anchor, stay shared pipe 0**
   This is the cheapest safe wire-format fix and should be landed even if Approach 3 follows. It makes sync association logically correct: a Tag will never apply another Tag’s anchor. But it does not make useful anchors reliably available, because the Hub still often misses TIFS and the next Tag may receive and discard the stale anchor.

   Expected p99: correctness improves, availability likely gets worse or visibly sparse. It prevents bad sync bias but probably does not reach fast-motion `<10 ms` p99 by itself.

3. **Approach 2: per-Tag pipes only**
   Incomplete as stated. ESB has max 8 pipes and the fleet has 10 Tags. If any pipe carries two Tags unscheduled, ambiguity remains. Also, without `rx_node_id`, bad provisioning or rollout mismatch can silently reintroduce wrong anchors.

   Expected p99: good only for truly one-Tag-per-pipe operation; otherwise not a full fix.

**Third Options**

TDMA/slot grants are the scalable correctness answer if 10 concurrent Tags matter. The Hub grants a Tag-specific uplink slot; the ACK payload for that slot is only meaningful for that Tag. This works even on shared pipe 0, because temporal ownership replaces pipe ownership. It is more invasive than `rx_node_id`, but it scales beyond 8 Tags and protects p99 better than best-effort ACK timing.

“Only fill `central_timestamp_us` on intended fast-path ACK, else zero” is a good safety mechanism, but not enough alone unless the Hub can prove intended delivery. On shared pipe 0 without Tag-specific ACK ownership, it mostly becomes an anchor-validity gate.

**Minimum Instrumentation First**

Land counters before the wire bump:

- Hub: `anchor_fast_path_written_before_ack_deadline`
- Hub: `anchor_slow_path_missed_tifs`
- Hub: `ack_payload_overwrite_or_pending_depth_by_pipe`
- Tag: `anchors_received`, `anchors_applied`, `anchors_rejected_wrong_node` after v4
- Tag/Hub: per-node anchor age and span histogram

Before v4, add Hub-side logging/counters estimating whether the ACK payload was queued inside the intended Tag’s response window versus after it. That gives the data needed without changing OTA format.

**Recommendation**

Ship **v4 with `rx_node_id` now**, plus instrumentation. Treat it as the correctness guardrail. Then implement **scheduled per-Tag ownership**: either true one-Tag-per-pipe where possible, or TDMA slotting for all 10 Tags. Do not rely on unscheduled shared pipes for sync.
