# Copilot — round 5 response (2026-04-22)

Session: Stage 3 findings + Stage 4 TDMA proposal.
Prompt: `round5_stage3_results_prompt_2026-04-22.md`

Invoked via `copilot -p` (GitHub Copilot CLI).
Tokens: ↑ 22.2k • ↓ 1.5k • 962 reasoning. 35 s wall.

---

## Copilot response

> **Call:** **Q1 defer**, **Q2 keep strict gate**, **Q3 do (b)**.

### Q1 — Retry-ambiguity hypothesis

> **Plausible, but probably not the main ≥30 ms tail, so don't
> land Stage 3.5 yet.** If the ring keeps the original `T_tx`, a
> later successful retry will bias the midpoint estimate, but
> that error should usually be **bounded to a few ms-class**, not
> explain the full residual ≥30 ms bucket by itself. The fact
> that **10–30 ms collapsed but ≥30 ms stayed flat** says
> midpoint canceled queue latency as expected, and the remaining
> tail is likely a mix of **retry timing, rare radio stalls,
> ISR/service jitter, or occasional bad pairings**. Best next
> step is **instrumentation first**: record retry count and
> "final successful TX time vs original TX time" for accepted
> anchors. If that shows retries dominate the tail, then Stage
> 3.5 is worth it; otherwise it's extra code for marginal gain.

### Q2 — Soft-accept neighbour anchors?

> **No soft accept in steady state.** Your current v4 gate is
> doing important work; relaxing it risks reintroducing exactly
> the contamination you just proved is dominant. At **~0.9 Hz**
> anchor rate and **1 ppm** drift, holdover is already excellent.
> If startup/reboot latency matters, do a **bootstrap-only
> coarse mode** with clearly lower confidence and automatic exit
> on first hard anchor, but do **not** make neighbour-derived
> soft accepts part of normal sync maintenance.

### Q3 — Stage 4 TDMA

> **Yes, choose (b): Stage 3 midpoint for acquisition, then
> transition to TDMA for refinement.** That is the safest path
> out of the chicken-and-egg problem: Stage 3 gives you a coarse
> shared timebase without schedule assumptions; TDMA then
> removes collisions and queueing, which is the only realistic
> path to **<1 ms p99**. Pure beacon-only TDMA bootstrapping is
> possible, but it creates a second sync mechanism before you've
> harvested the value from the one that already works. A sparse
> Hub beacon can still help later as a **maintenance / relock
> aid**, but it should complement, not replace, Stage 3
> bootstrap. Per-Tag pipes are orthogonal; they may reduce
> contamination, but they do **not** buy you deterministic
> airtime, so they're not the answer to the p99 target.
