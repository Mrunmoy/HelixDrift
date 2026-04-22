# Codex — round 6 response (2026-04-22)

Session: Consolidated RF decisions D1–D6 from `docs/RF.md`.
Prompt: `round6_consolidated_prompt_2026-04-22.md`
Piped RF.md (709 lines) as context.

Invoked via `codex exec`. Tokens: 42,397.

---

## Codex response

**Top pick:** start Stage 4 beacon TX now, while running v17 soak
whenever the fleet is otherwise idle.

### D1 — Is <1 ms p99 a hard v1 requirement?

> Treat `<1 ms p99` as a hard v1 requirement for the RF
> architecture. Stage 3.6 is a good fallback/baseline, but
> `53.5 ms p99` is already known-bad for fast motion, and PC
> fusion cannot reliably "discover" away RF timestamp skew after
> the fact.

### D2 — TDMA vs per-Tag pipes

> TDMA is the answer. Per-Tag pipes are blocked by 8 ESB pipes
> vs. 10 Tags, and grouped pipes reintroduce the same FIFO
> contamination. Multi-frequency adds coordination complexity
> without solving timestamp determinism as cleanly. Stage 3
> acquire → beacon refine → TDMA slots is the right path.

### D3 — Equalise retransmit_delay?

> Do not equalise `retransmit_delay` as a mainline fix. It might
> buy ~3 ms bias spread, but that is irrelevant against a 53 ms
> span and risks undoing the Phase C throughput win. Run the A/B
> only after beacon/TDMA risk is retired or if you need a
> low-effort filler test during a blocked window.

### D4 — "RF done" blocker bar

> Multi-hour soak, Tag mid-stream reset, and at least one
> body-shadowing/worn-layout capture. Thermal drift can be
> covered by the soak unless results suggest otherwise. 2.4 GHz
> coexistence, battery sag, and charge-while-streaming are
> hardening tests, not blockers for declaring the RF
> architecture complete.

### D5 — Retry instrumentation vs Stage 3.5 code

> Agree with Copilot's path: instrument retry timing only, no
> behavior change yet. The 1.4 % `>=30 ms` bucket is not the
> architectural limiter; touching retry behavior before TDMA
> risks spending time on a secondary effect.

### D6 — Overnight order

> 1. `c` Stage 4 Hub beacon TX, beacon-only, no Tag behavior
>    change. This is the critical path for sub-ms and gives early
>    evidence on beacon jitter/propagation without disturbing
>    v17 sync.
> 2. `d` Run a multi-hour v17 or beacon-only soak overnight,
>    depending on when beacon lands. Capture normal sync metrics
>    plus beacon telemetry if available.
> 3. `a` Add retry-timing instrumentation only if beacon TX
>    lands cleanly and there is still focused firmware time.
> 4. `b` Defer retransmit-delay A/B. It is a useful cleanup
>    experiment, but it cannot close the requirement gap.

> Why: the current data already answers the architecture
> question. The next scarce resource should go into de-risking
> TDMA bootstrap, not optimizing Stage 3.6's remaining
> tens-of-ms failure mode.
