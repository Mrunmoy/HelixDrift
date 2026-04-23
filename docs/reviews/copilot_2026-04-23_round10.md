
**Why NOT to use EMA:**
- None. The v22 findings show raw midpoint updates (no filtering) give similar span to glitch-reject, but EMA is strictly safer than binary accept/reject.

---

### Q5: Surprising Findings & Next Experiment

**Surprising finding:**
**Glitch-reject made things WORSE by preventing recovery from bad initial locks.** This is counterintuitive — the hypothesis that "1.8% of samples cause the fat-tail" was plausible, well-reasoned, and backed by instrumentation. Yet the experiment refuted it decisively. The lesson: **single-sample filtering is fundamentIally incomp’veatible with d got the findings and one firmwarerifting baselines.** Systems review; the architecture review is still running, and I want that perspective before I give you the team answer. that assume I’m waiting on that now. "good baseline + outliers" fail when the baseline itself is the problem.

**Recommended next experiment:**

**"PC-side per-Tag bias compensation validation"** — prove that the drift hypothesis is correct and that sub-ms is

 achievable TODAY.

**Procedure:**
1. **Capture 5-minute baseline** with all Tags on v21 (revert v22, keep Stage 4 TDMA for fleet-bias stability).
2. **Compute per-Tag mean bias** over the first 60 seconds (calibration phase). Store in a lookup table `bias_table[node_id]`.
3. **Apply bias correction PC-side**: For each incoming frame, compute `sync_us_corrected = sync_us - bias_table[node_id]`.
4. **Measure cross-Tag span p99 on corrected data**. Hypothesis: should drop from ~45 ms to <5 ms.

**Expected outcomes:**
- **If span p99 < 5 ms**: Drift hypothesis confirmed. PC-side compensation is the path. Ship a calibration tool and declare sub-ms sync achieved (PC domain).
- **If span p99 still ~40 ms**: Drift hypothesis partially wrong. Investigate whether the fat-tail is actually hub-side FIFO depth variation (per v20's ack_lat histogram showing 51% ≥30 ms). This points to a hub firmware architecture change (FIFO depth 1 or hardware TX timestamp).

**Why this experiment:**
- **Cheap**: 1-2 hours of PC script work, no firmware flashing, no OTA risk.
- **Decisive**: Either proves the architectural path (PC-side) or narrows the remaining firmware problem to hub FIFO.
- **Actionable**: If it works, you have a deployable solution. If it doesn't, you have a specific next target (hub FIFO depth).

---

## Summary

| Question | Answer |
|----------|--------|
| Q1: Drift vs. Jump? | **DRIFT.** Cross-Tag span didn't improve despite filtering 1.8% of jumps. Fat-tail is systematic per-Tag bias drift. |
| Q2: PC-side or firmware? | **PC-side per-Tag bias compensation.** Firmware should focus on initial-lock quality; PC owns sub-ms via calibration. |
| Q3: Initial lock gate? | **5 consecutive samples within ±3 ms**, with 10-sample retry backoff on failure. Prevents bad baselines like Tag 3's +2.57s lock. |
| Q4: EMA help or hurt? | **HELPS.** Use α=0.2 (51/256 fixed-point). Smooths noise, tracks drift, no catastrophic failure mode unlike glitch-reject. |
| Q5: Surprising finding? | Glitch-reject broke recovery. **Next: validate PC-side bias compensation in 5-min capture** — should drop span p99 to <5 ms. |

**Bottom line:** v22 refuted the outlier hypothesis but illuminated the correct architecture. The path to sub-ms is PC-side per-Tag bias tables, not firmware filtering. Ship a calibration tool and call it done.The architecture pass just finished. I’m pulling that in and then I’ll give you a concise reviewer-style answer to each question.

● Read (Design-reviewer agent — Review sync architecture)
  └ Completed

**Reviewer 1 — Firmware**

**Q1:** Yes. v22 is strong evidence against the jump/outlier hypothesis. If `>=10 ms` single-sample jumps were the main cause, p99 cross-Tag span should have collapsed. Instead p50 and p99 both stayed large/worsened. That pattern fits **persistent per-Tag bias/drift**, not rare spikes. The fact that the p50 gap is already huge is the strongest clue.

**Q2:** The sub-ms path is mostly **PC-side per-Tag bias compensation**. Firmware can keep each Tag “locally clean,” but it cannot remove board-specific crystal/ISR/RF asymmetry without an external reference. I don’t see a firmware-only lever likely to buy the last order of magnitude unless you change the architecture around timestamping.

**Q3:** Yes, the right fix shape is **initial lock qualification before baseline commit**. I’d do:
1. candidate baseline from first sample,
2. require **5 consecutive** samples within **±3 to ±5 ms** of that candidate,
3. only then mark locked,
4. if the streak breaks, restart acquisition,
5. after repeated failures, raise a diagnostic / force reacquire.

That would have prevented the Tag 3 failure mode.

**Q4:** I would **not** use EMA as the main protection. It smooths noise, but it also smears wrong baselines and step changes. It doesn’t solve bad initial lock; it just makes recovery slow. A small post-lock median/consistency check is safer than EMA if you want mild smoothing.

**Q5:** The surprising part is not just “glitch reject failed,” but **how** it failed: the state machine assumed “good baseline + occasional bad samples.” Your data show the opposite can happen: “bad baseline + mixed stream,” and then rejection becomes a trap. That’s the real lesson.

**Next experiment:** revert to v21/v23 behavior, add **initial-lock gate only**, no glitch reject, then test whether stuck-tag failures disappear while fleet span remains unchanged. That isolates “startup robustness” from “drift correction.”

---

**Reviewer 2 — Systems / Architecture**

**Q1:** Agreed. The preserved scaling between per-Tag error and fleet span at both p50 and p99 looks like a **bias-distribution problem**, not a contamination-tail problem. v22 effectively falsified the idea that filtering isolated jumps fixes the fleet-level span.

**Q2:** Architecturally, yes: **Hub/PC should be authoritative**, and the PC should subtract a measured per-Tag bias before fusion. That is the cleanest separation of concerns:
- firmware: stable, observable timestamp estimator,
- PC: fleet calibration and bias compensation.

A firmware-only solution would need a stronger shared timing reference or hardware-grade TX/RX timestamping. Without that, you’re mostly rearranging estimator behavior.

**Q3:** Yes, require a **qualified initial lock**. I’d recommend:
- **N = 5** consecutive anchors,
- **tolerance = ±3 ms** around the running candidate,
- on failure, reset the candidate immediately,
- after several failed attempts, declare “not locked” rather than silently accepting garbage.

Important: do **not** let one accepted near-baseline sample indefinitely preserve a bad baseline. Acquisition and tracking should be separate states.

**Q4:** EMA has limited value. It is fine as a **small post-lock smoother** if you want prettier local traces, but it is not a correctness mechanism. It won’t fix systematic bias, and if overused it hides state changes and slows recovery. If used at all, keep alpha low and only after lock qualification.

**Q5:** The key surprise is that the experiment gave you a clearer architectural boundary: **don’t fight fleet bias inside the Tag estimator**. Use firmware to prevent pathological lock-in and expose telemetry; use PC calibration to remove systematic offsets.

**Next experiment:** take one clean capture on v23, compute a **per-Tag median bias over the first 60 s**, subtract it offline, and re-measure cross-Tag span. If span collapses hard, you’ve validated the architecture without another firmware cycle.


Changes   +0 -0
Requests  1 Premium (2m 55s)
Tokens    ↑ 489.4k • ↓ 10.1k • 398.8k (cached) • 2.6k (reasoning)
