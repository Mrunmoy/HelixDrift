# Stage 3 design — v5 anchor with midpoint estimator

Status: **design draft** (not yet implemented). Target after Stage 2
(v4 anchor `rx_node_id`) measurement confirms expected rejection
ratio.

Parent: [RF_SYNC_DECISION_LOG.md](RF_SYNC_DECISION_LOG.md)

## Goal

Collapse cross-Tag sync span from p99 ~50 ms to **p99 < 5 ms** by
eliminating the two remaining systematic biases:

1. **Hub-side ACK-TX latency bias.** Stage 1' histogram shows >70 %
   of ACK payloads are transmitted 10-30 ms after the Tag's frame
   arrived. Hub currently stamps `central_timestamp_us = rx_time`,
   not TX time — so Tags using that stamp to compute offset pick up
   the queue-latency as sync error.

2. **Tag-side ACK-RX uncertainty.** Tag records `local_us` when the
   anchor packet arrives — but doesn't know which of its recent TXs
   this anchor was actually built from (retry round-trip
   ambiguity). Tag can't compensate.

Fix: **midpoint round-trip estimator.**

## Wire format — v5 anchor (16 bytes)

```c
struct __packed HelixSyncAnchor {
    uint8_t  type;                   // 0xA1
    uint8_t  central_id;
    uint8_t  anchor_sequence;
    uint8_t  session_tag;
    uint32_t central_timestamp_us;   // Hub's RX time (kept for back-compat)
    uint8_t  flags;
    uint8_t  ota_target_node_id;
    uint8_t  rx_node_id;             // v4
    uint8_t  rx_frame_sequence;      // v5 NEW
    uint32_t anchor_tx_us;           // v5 NEW — Hub's clock just before
                                     //          esb_write_payload()
};  // 16 bytes
```

Backward compat: 8/9/10/11/16-byte gates in `node_handle_anchor()`.
A v5 Tag on a v4 Hub accepts the v4 (11-B) path and runs the Stage-2
filter without midpoint correction.

## Midpoint estimator

Given:
- `T_tx_us`: Tag's local time when the frame was queued for TX
  (looked up via rx_frame_sequence → ring buffer on Tag)
- `T_rx_us`: Tag's local time when the anchor packet arrived
- `H_rx_us`: `central_timestamp_us` — Hub's clock at frame RX
- `H_tx_us`: `anchor_tx_us` — Hub's clock at ACK TX

Round-trip symmetry assumption (same propagation each way,
~1 µs each):

```
Tag local midpoint = (T_tx_us + T_rx_us) / 2
Hub local midpoint = (H_rx_us + H_tx_us) / 2
offset             = Tag_midpoint - Hub_midpoint
```

Beauty of midpoint: queue-latency on either side cancels exactly as
long as the clock offset is stable over the ~10-30 ms round trip
(which it is — 1 ppm drift = 30 ns over 30 ms, negligible).

## Tag-side ring buffer

```c
#define TAG_TX_RING   16u
#define TAG_TX_MASK   (TAG_TX_RING - 1u)

struct tx_stamp {
    uint8_t  sequence;
    uint32_t local_us;
};

static volatile struct tx_stamp tx_ring[TAG_TX_RING];
static volatile uint32_t tx_ring_head;
```

Push on successful `esb_write_payload()` (in `maybe_send_frame()`);
lookup on anchor RX by walking tail→head for matching sequence.

## Why `rx_frame_sequence` is necessary

NCS ESB driver on the PTX side fires `ESB_EVENT_TX_SUCCESS` when an
ACK is received, but does **NOT** tell the Tag which of its
in-flight retries was ACK'd. With `retransmit_count = 10` and
potential collisions, the Tag's "most recent TX" vs. "the one that
actually got ACK'd" can differ by several retry slots.

`rx_frame_sequence` eliminates that ambiguity by round-tripping the
Tag's own sequence number through the Hub — Hub reads
`frame->sequence` on RX and echoes it back in the anchor. Tag looks
up its matching TX timestamp in the ring.

Without `rx_frame_sequence`, the midpoint estimator's
`T_tx_us` input is wrong by up to N × retransmit_delay (N × ~600 µs
= several ms). That defeats the whole point of Stage 3.

## Instrumentation additions

Tag:
- `offset_jitter_bucket[4]`: |midpoint_offset - EMA| distribution.
  Validates that the estimator is consistent.
- `seq_lookup_miss`: counter incremented when rx_frame_sequence
  isn't in the Tag's ring (too old — anchor arrived after ring
  wrapped). Should be near-zero in practice.

Hub:
- No new instrumentation — Stage 1' ring buffer stays.

## Validation plan

1. Flash Hub + fleet OTA all Tags to v15.
2. 15-min SUMMARY capture alongside frame-level gap analysis from
   `analyse_mocap_gaps.py`.
3. Key metrics:
   - Cross-Tag span p50, p99 (target: p99 < 5 ms)
   - Per-Tag offset_step p99 (target: < 2 ms)
   - seq_lookup_miss ratio (target: < 0.1 %)
4. If p99 still > 5 ms after Stage 3, escalate to Stage 4 (TDMA
   slots) — see decision log.

## Rollback

If Stage 3 regresses (e.g., seq_lookup_miss >> expected), Tags fall
back to v4 behaviour by ignoring the midpoint fields and using
`central_timestamp_us` alone. Fleet can be rolled back with a
`fleet_ota.sh 1 16` to any earlier version (MCUboot OVERWRITE_ONLY
accepts downgrades).
