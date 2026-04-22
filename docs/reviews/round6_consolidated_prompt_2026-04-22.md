# Round 6 prompt — consolidated RF ask (2026-04-22)

Please read `docs/RF.md` end to end before answering. It is the
single consolidated status doc for the RF sprint: what's built,
what's measured, what's open, and what's proposed. All earlier
narrative docs are archived under `docs/archive/rf/`.

Your job: **pick a direction on D1–D6 in §8 and propose an ordered
execution plan for the next 1–3 days of development**. I'm going
to lead overnight development based on the group consensus.

For context, the state today (2026-04-22) is:
- Fleet: 10 Tags at v17 (Stage 3.6), Hub with Stage 3 v5 anchor.
- 20-min validation: per-Tag |err| p99 13.5–17.5 ms, cross-Tag
  span p99 53.5 ms, fleet bias -6.6 ms ± 1.8 ms Tag-to-Tag.
- Requirements doc targets < 1 ms p99. Gap ~50×.
- Both you and the other reviewer (round 5) agreed on TDMA
  bootstrap path (b) — Stage 3 acquires, TDMA refines.

Specific asks:

**D1.** Is < 1 ms p99 cross-Tag span actually a hard v1 requirement,
or can we ship Stage 3.6's 53 ms p99 and tighten later if PC-side
fusion complains? User has said *both* slow (rehab/VR) and fast
(sports/dance) mocap are on the road-map, but PC-side fusion
hasn't been built yet — so real-world tolerance isn't measured.

**D2.** If D1 is "hard yes": confirm TDMA is the answer (not
per-Tag pipes or some third option we haven't considered).

**D3.** `retransmit_delay = 600 + 50·node_id µs` creates a
~2 ms bias clustering between Tags 1–3 and 4–10. Should we
equalise (all same delay) to tighten fleet-wide bias, at the
risk of re-introducing collision hotspots that Phase C solved?
A/B test plan in §8.D3.

**D4.** What's the test bar for "RF is done"? Currently untested:
multi-hour soak, thermal drift, 2.4 GHz coexistence, battery
sag, charge-while-streaming, body shadowing, Tag mid-stream
reset. Which are blockers vs. deferrable hardening?

**D5.** Residual 1.4 % ≥30 ms bucket in Stage 3 `mid_step`. Round
5 advice split: Codex said land Stage 3.5 narrowly, Copilot
said instrument first and only land if retries prove to
dominate. I'm taking Copilot's path — instrument only,
no code change yet. Agree?

**D6.** Ordered execution plan for overnight work. Options on the
table (pick an order or propose a different one):
  a. Instrument retry-timing on v18 (no behaviour change, just
     telemetry) — 2 hours firmware + fleet OTA + capture.
  b. A/B retransmit_delay equalisation test (D3) — 1 hour fw
     + fleet OTA + 20-min capture + analysis.
  c. Start Stage 4 beacon TX on Hub (beacon-only, no Tag
     changes yet) — measures beacon propagation + jitter,
     doesn't disturb existing sync — 3-4 hours firmware + capture.
  d. Multi-hour soak with v17 as-is (D4) — costs no firmware
     work, just wall-clock. Could run overnight alongside other
     work.

Under 500 words. Give me your top pick + 1-2 sentences why.
Overnight execution will follow the group's ordered plan.
