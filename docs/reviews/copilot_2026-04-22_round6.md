# Copilot — round 6 response (2026-04-22)

Session: Consolidated RF decisions D1–D6 from `docs/RF.md`.
Prompt: `round6_consolidated_prompt_2026-04-22.md`
(Copilot read the RF.md itself via its filesystem tool.)

Invoked via `copilot -p`. Tokens: ↑ 109.4k • ↓ 2.0k
• 96.6k cached • 938 reasoning. 54 s wall.

---

## Copilot response

**Direction:** treat **<1 ms p99 cross-Tag span as a hard v1
requirement** and commit to the **TDMA bootstrap path**. Stage 3.6
is a good slow-motion / bench-demo baseline, but at **53.5 ms p99**
it is too far from the stated target and too risky for
fast-motion mocap to call "ship-ready."

### D1 — Hard yes

> The doc's own product framing says fast motion is in-scope and
> 50 ms p99 implies unusable joint-angle error there. Since PC
> fusion does not exist yet, I would not bet v1 on an unmeasured
> downstream rescue.

### D2 — TDMA is the answer

> Per-Tag pipes are structurally blocked by the **8-pipe limit
> vs 10 Tags**; grouped pipes reintroduce the FIFO problem. I do
> not see a cleaner third option than **Stage 3 acquire / Stage
> 4 TDMA refine**.

### D3 — Do not change default retransmit_delay yet

> Equalising may buy only **~3 ms** on a **~53 ms** problem,
> while risking the Phase C throughput win. Run the A/B test
> only as a low-priority side experiment after instrumentation,
> not as the main overnight bet.

### D4 — "RF done" blocker bar

> **Blockers before calling RF done:**
> - multi-hour soak
> - Tag mid-stream reset recovery
> - one realistic body-shadowing / worn-motion sanity run
> - one 2.4 GHz coexistence sanity run
>
> **Deferrable hardening:**
> - formal thermal characterization
> - battery sag sweep
> - charge-while-streaming
>
> unless battery-powered untethered use is imminent for v1.

### D5 — Instrument only

> Yes, instrument only. I agree with your Copilot-leaning call:
> do **retry-timing telemetry first, no behaviour change**. The
> residual **1.4 % ≥30 ms** bucket is too small to justify
> another narrowing patch before proving causality.

### D6 — Ordered plan

> 1. **a. Instrument retry timing on v18** and capture a 20-min
>    fleet run.
> 2. **d. Start a multi-hour soak in parallel overnight** on
>    the best-known build.
> 3. **c. Start Stage 4 beacon TX on Hub** immediately after
>    the instrumentation capture; make this the first real
>    architectural step.
> 4. **Only then consider b. retransmit_delay A/B**, and only
>    if telemetry says bias cleanup is worth chasing before
>    Tag-side beacon/EMA work.

> **Top pick: a → d (parallel) → c; defer b.**
>
> Why: it preserves momentum toward the only architecture that
> can plausibly close the **50× gap**, while cheaply resolving
> the last Stage 3 unknown and adding blocker-grade reliability
> evidence overnight.
