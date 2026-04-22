# Copilot — round 4 response (2026-04-22)

Session: Stage 2 98.6 % rejection-ratio surprise + v5 design check.
Prompt: `round4_stage2_measurement_prompt_2026-04-22.md`

Invoked via `copilot -p`. Tokens: ↑ 106.0k • ↓ 1.7k • 95.7k cached
• 535 reasoning. 50 s wall.

---

## Copilot response

Copilot first inspected the landed v4 anchor code
(`zephyr_apps/nrf52840-mocap-bridge/src/main.cpp`) to verify the
implementation before answering.

### Q1 — Is the Stage 2 98.6 % rejection-ratio interpretation right?

> **Yes — mostly right.** If `rejection_ratio` comes in **low**
> (<30%), that means **wrong-Tag delivery is not the dominant
> source** of the bad Stage 1' latency histogram. In other words,
> most delayed ACK payloads are still carrying anchors for the
> **same Tag** that ultimately receives them. I would **not**
> narrow that to only "the predecessor" — it could be
> predecessor, predecessor-2, etc. The key conclusion is:
> **queue latency is real, but cross-Tag contamination is not
> the main delivered-path failure mode**.
>
> **Follow-up:** treat that as evidence that Stage 2 fixed only
> one class of corruption, and go straight to **Stage 3 exact
> pairing**. Also compare post-v4 `offset_step` collapse vs
> `wrong_rx`; if `wrong_rx` is low but `offset_step` still has a
> fat tail, the remaining problem is **same-Tag stale-anchor
> pairing**, not contamination.

### Q2 — v13/v14 fleet-mixed rollout concern?

> **Not a serious migration concern.** I'd classify the
> v13-during-rollout window as **acceptable / "who cares, sync
> was already not trustworthy there."** v13 Tags were already
> consuming mispaired anchors; v4-on-Hub does not create a new
> semantic hazard for them, it just fails to give them the new
> protection because they can't see `rx_node_id`. So I would not
> worry about "sync continuity" in the strong sense during that
> 40-minute window. I **would** avoid drawing comparative
> sync-quality conclusions from mixed v13/v14 fleet captures
> during rollout.

### Q3 — Is `rx_frame_sequence` strictly necessary?

> **Yes — `rx_frame_sequence` is necessary.** Keep it.
>
> Your reading of the ESB semantics is correct enough for design:
> after v4, the remaining ambiguity is no longer **which Tag**,
> but still **which TX of that Tag** the anchor corresponds to.
> Since the Tag does **not** get authoritative feedback
> identifying the exact successful transmitted frame/retry
> instance, "last TX" is not a safe pairing key under retries /
> queueing. Without `rx_frame_sequence`, your midpoint estimator
> can still pair the anchor with the wrong local TX timestamp
> and reintroduce exactly the bias/jitter you're trying to remove.

### Summary

> 1. **Q1:** **Yes**, low rejection means Stage 1' was mostly
>    queue latency, not mostly cross-Tag contamination — but
>    possibly across multiple same-Tag stale anchors, not just
>    predecessor.
> 2. **Q2:** **No major concern**; acceptable during rolling
>    upgrade.
> 3. **Q3:** **Yes, `rx_frame_sequence` is necessary.**
