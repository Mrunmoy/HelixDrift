# Codex — round 5 response (2026-04-22)

Session: Stage 3 findings + Stage 4 TDMA proposal.
Prompt: `round5_stage3_results_prompt_2026-04-22.md`

Invoked via `codex exec` (OpenAI Codex CLI, logged in via ChatGPT).
Tokens used: 24,227.

---

## Codex response

> I'll answer from the firmware/sync design angle and keep it
> concise. I'm not going to browse here since this is
> repo-specific/offline engineering judgment, but I'll ground it
> in the measurements you gave.

### Q1 — Residual 1.4 % `>=30 ms` bucket / Stage 3.5 scope

> Retry ambiguity is plausible, but not enough to explain all
> `>=30ms` by itself. A stale original `T_tx` over 10 retries
> gives single-digit ms error, not 30ms-class unless it combines
> with ESB scheduling/FIFO variance or delayed ACK association.
> Still, Stage 3.5 is worth landing because it is small, local,
> and improves timestamp truth: update the TX ring entry on every
> retry attempt, or record `seq -> last_actual_tx_us` for the
> attempt that receives ACK. Treat this as cleanup plus
> instrumentation, not the primary fix for the 1.4 %.

### Q2 — Soft-accept neighbour anchors?

> Do not soft-accept neighbour anchors into the main clock
> discipline yet. The v4 seq-indexed gate is doing valuable
> contamination rejection, and relaxing it risks reintroducing
> the exact cross-tag bias Stage 3 removed. Better path: keep
> foreign/near anchors as diagnostic or low-confidence "candidate
> observations" only, behind a separate counter/histogram. For
> reboot/idle latency, prefer forcing a Tag-originated sync
> probe after boot or after stale-sync timeout. Own-seq
> acceptance should remain the authority.

### Q3 — Stage 4 TDMA bootstrap approach

> Agree with path `(b)`: bootstrap with Stage 3 midpoint sync,
> then transition into TDMA once locked. It avoids a hard
> dependency on beacon acquisition, uses the working estimator,
> and lets TDMA become a refinement layer rather than the source
> of first truth. Add hysteresis: Stage 3 open contention until
> lock, TDMA while lock is fresh, fall back to Stage 3 if lock
> ages out or slot misses exceed threshold.

### Overall recommendation

> I would land Stage 3.5 if it is narrowly scoped: retry
> timestamp correction plus counters proving whether `>=30ms`
> shrinks. Then proceed to Stage 4. The big p99 win likely comes
> from TDMA eliminating collision/queue dynamics, not from
> further estimator tuning.
