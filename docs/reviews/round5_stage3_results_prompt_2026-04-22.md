RF sprint round 5 — Stage 3 v5 anchor (midpoint RTT estimator)
landed, measured.

Commit: acf315b  m8-rf: Stage 3 findings — midpoint jitter 30x
                 reduction in 10-30ms bucket

Tag 1 SWD-read of in-RAM histograms after 10-min v15 capture on all
10 Tags:

  anchors_received (v4-accepted):             518
  anchors_wrong_rx (v4-rejected):          32,406
  seq_lookup_miss:                              0

  mid_step (v5 midpoint RTT) vs. old offset_step (central_ts only):
    <2ms      58.4%  vs.  46.6%   (+25%)
    2-10ms    39.9%  vs.  39.8%   (flat)
    10-30ms    0.4%  vs.  12.0%   (30x reduction)
    >=30ms     1.4%  vs.   1.5%   (flat)

The predicted Hub-side ACK-TX queue-latency bias (Stage 1' showed
>70% of ACK TXs land 10-30ms late) was exactly what midpoint math
cancelled. Confirmed: with central_timestamp_us AND anchor_tx_us
paired against the Tag's own T_tx (via rx_frame_sequence → ring
lookup), the round-trip cancels queue latency on both sides.

Three questions for the group:

Q1. The residual 1.4% in >=30ms bucket — I hypothesise this is
    Tag-side retry ambiguity: when Tag retransmits N times before
    ACK, the ring entry for rx_frame_sequence matches the ORIGINAL
    TX time, not the retry that actually got through. At
    retransmit_delay=600µs + N*50µs and N up to 10, T_tx is up to
    ~6ms too early. After doubling through the midpoint math, that
    shows as up to ~3ms error — but the ESB TX path can have more
    variance than that. Is this hypothesis right, and is it worth
    Stage 3.5 (update ring entry on each retry) or defer?

Q2. Effective anchor-update rate is ~0.9 Hz (Stage 2's v4 filter
    gates 98.6% of anchors). At 1ppm clock drift, 1Hz sync updates
    leave <1µs drift between updates — plenty. But a Tag that
    hasn't TXed in a while (or just rebooted) takes ~1 sec to get
    a fresh anchor, longer if cross-contamination ratio worsens
    under lower traffic. Should we keep a "soft accept" path for
    anchors built from our neighbours but within N retransmit
    periods, treating them as low-confidence updates? Or let the
    Tag's own seq-indexed accept rate be the only gate?

Q3. We're ~5x better than Phase C (p99 dropped from ~50ms to
    effectively ~10ms). The v1 aspirational target is <1ms p99.
    Stage 4 is TDMA scheduled slots: each Tag TXes only in its
    assigned N-ms slot (slot width = ~2ms, 10 Tags × 2ms slots =
    20ms schedule). Eliminates collisions; eliminates FIFO queue
    (since at any moment only one Tag is TXing to Hub).

    But TDMA scheduling needs a sync source — chicken-and-egg
    with the sync layer. Two patterns:
      (a) Hub broadcasts slot beacons that Tags align against. Tags
          drift between beacons but hold sync to within drift*beacon_period.
      (b) Bootstrap with Stage 3 midpoint sync as the alignment
          source; TDMA refines. Tags start in Stage 3 mode, once
          sync is locked they transition to TDMA.

    I lean toward (b) — does the group agree? Or is there a third
    path I'm missing (e.g. per-Tag ESB pipes if we abandon the
    10-Tag constraint and go to 8 Tags max)?

Give us a concise call on all three, under 400 words. User is
working offline; I have authority to land Stage 3.5 or Stage 4 if
the group agrees the improvement is worth the code.
