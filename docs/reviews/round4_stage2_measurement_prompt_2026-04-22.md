RF sprint round 4 — Stage 2 v4 anchor landed. Committed as:

  ae3cc97  m8-rf: Stage 2 — v4 anchor with rx_node_id (shared-pipe sync fix)

Your round-3 feedback said: "fix the instrumentation bias (Option-A
ring buffer), but the distribution shape still warrants Stage 2
correctness work in parallel." Done:

  - Hub ring buffer landed in commit 0d9708a (Stage 1').
  - Stage 1' Hub capture: <2ms = 2.1%, 2-10ms = 26.5%, 10-30ms = 63.1%,
    >=30ms = 8.3% — unbiased distribution now shows >70% of anchor ACK
    TXs are on the 10-30ms or slower path.
  - Tag 1 SWD-read of in-RAM histograms revealed that anchor_age on
    Tag is NOT a staleness measure — it measures the ESB Tag-TX to
    Tag-ACK-RX round trip, which hardware keeps fast (~1ms) regardless
    of what content attached. The signal I should have been tracking
    was offset_step: |Δ estimated_offset|. That shows 477 events in
    10-30ms bucket and 11 events in >=30ms bucket out of 59k anchors —
    clear evidence of cross-Tag anchor corruption.

Stage 2 just landed (ae3cc97). Summary of what's in it:

  Wire format v4 (11 bytes):
    type | central_id | anchor_sequence | session_tag |
    central_timestamp_us | flags | ota_target_node_id |
    rx_node_id   ← NEW in v4

  Hub sets anchor.rx_node_id = frame->node_id when building the ACK
  payload. Tag's node_handle_anchor() filters: if payload.length >= 11
  and rx_node_id != 0xFF and rx_node_id != g_node_id, increment
  anchors_wrong_rx and RETURN without touching estimated_offset_us.
  OTA flag processing is unaffected (it runs before the filter and
  has its own target_node_id gate).

  SUMMARY extended with wrong_rx=N on the node role so we can compute
  rejection ratio: wrong_rx / (anchors + wrong_rx).

  Backward compat: v4 Tag accepts v3 (10-B) anchors too — no
  rx_node_id field means accept-all, preserving today's behaviour
  during a v3 Hub + v4 Tag migration.

Currently mid-fleet-OTA to v14. 10-min capture coming after OTA
completes (~40 min). Three questions for the group:

Q1. Based on the Stage 1' Hub histogram (>70% of ACK TXs in 10-30ms
    or slower bucket), I'm expecting rejection_ratio ≈ 70-90% on each
    Tag. If rejection comes out much lower (say <30%), what would
    that tell us? My read: low rejection = FIFO drains fast enough
    that the ACK-payload attached to each frame really was built
    from that frame's predecessor (same Tag). In which case, the
    Stage 1' Hub measurement is really showing queue-latency not
    cross-contamination. Does that sound right, and what's the
    follow-up?

Q2. Backward-compat strategy. Today: the fleet is all v13 (pre-v4).
    Upgrade plan: flash Hub FIRST (already done via SWD), then OTA
    Tags. During the ~40 min in-between, v14 Tags that aren't yet
    flashed are still on v13 code that treats ANY anchor (including
    v4) as valid and updates the estimator unconditionally. So for
    ~40 min the v13 Tags get MORE cross-contamination than before
    (because Hub is now broadcasting rx_node_id bytes that v13
    doesn't even see, but the underlying ESB behaviour is unchanged).
    Is this a concern for sync continuity during the rolling
    upgrade, or is this "who cares, sync was already broken"?

Q3. Stage 3 design (v5 anchor, 16 bytes). Planned fields:
      + uint8_t rx_frame_sequence;  // which TX of mine this anchor is for
      + uint32_t anchor_tx_us;       // Hub's clock right before
                                     // esb_write_payload — the TX-side
                                     // half of the midpoint estimator
    Tag keeps a small ring buffer {tx_sequence, tx_local_us} and on
    anchor RX:
      tx_us = ring.find(anchor.rx_frame_sequence).local_us
      offset = (tx_us + anchor_rx_local_us) / 2  -  anchor_tx_us
    This gives true midpoint sync (removes both TIFS-variance bias AND
    the ACK-TX latency bias from Stage 1').

    Question: is rx_frame_sequence strictly necessary once v4 is
    active? After Stage 2, we already drop wrong-Tag anchors. The
    remaining Tag-side uncertainty is: "the Hub built this anchor
    from SOME frame of mine, but which one?" In principle the Tag
    always knows its last-TXd sequence (the one that just ACK'd),
    so rx_frame_sequence might be redundant.

    Counter-argument: NCS ESB on the Tag side doesn't give the Tag
    the sequence number of the frame that just got ACK'd. The Tag
    sees TX_SUCCESS events but doesn't know which of its in-flight
    retries actually made it. If the Tag is on retry 3 of 6 and the
    Hub gets one of them, the Tag's "last_tx_sequence" might be off
    by 0-5. rx_frame_sequence would disambiguate.

    Is that the right reading of the ESB driver semantics, or do I
    have this wrong?

Reply concise (<500 words), definitive yes/no on Q1 interpretation
and Q3 rx_frame_sequence necessity.
