# Codex — round 4 response (2026-04-22)

Session: Stage 2 98.6 % rejection-ratio surprise + v5 design check.
Prompt: `round4_stage2_measurement_prompt_2026-04-22.md`

Invoked via `codex exec`. Tokens used: 30,863.

---

## Codex response

> I'll verify the ESB event semantics against primary docs before
> giving a definitive answer, because Q3 hinges on whether
> TX_SUCCESS carries payload identity.
> (web searched: NCS ESB TX_SUCCESS event payload sequence number)

### Q1 — Is the Stage 2 98.6 % rejection-ratio interpretation right?

> **Yes, your interpretation is right.** If
> `wrong_rx / (anchors + wrong_rx)` comes out <30%, Stage 1' was
> mostly measuring ACK-payload queue latency, not cross-Tag
> contamination. The follow-up is then Stage 3 anyway: same-Tag
> stale anchors still bias sync because the Hub timestamp belongs
> to an earlier frame. Also verify v4 parsing with a per-Tag
> Hub-side counter: `ack.rx_node_id == current frame->node_id`
> vs not, so the Tag rejection ratio has a Hub-side cross-check.

### Q2 — v13/v14 fleet-mixed rollout concern?

> **Not a major concern.** v13 Tags are already vulnerable to the
> same ESB ACK-payload attachment behavior. The extra v4 byte
> does not make them semantically worse if their parser reads
> only the v3 prefix and ignores length beyond 10. During the
> rollout, v13 Tags remain in the old broken sync mode; upgraded
> v14 Tags improve as soon as they see v4 anchors. The only real
> concern is compatibility breakage, not sync continuity.

### Q3 — Is `rx_frame_sequence` strictly necessary?

> **Yes, keep `rx_frame_sequence`.** v4 proves "this anchor is
> for my Tag," but not "which one of my TX attempts/frames
> produced it." ESB `TX_SUCCESS` tells you an ACK was received;
> it does not give you an application payload sequence. You can
> infer it only if your Tag code guarantees exactly one logical
> frame in flight and stable sequence ownership across retries.
> That is too implicit for the midpoint estimator.

> For v5, `rx_frame_sequence` is the right disambiguator. It
> makes the estimator auditable, handles retries cleanly, and
> lets you reject anchors whose referenced TX is no longer in
> the Tag ring.
