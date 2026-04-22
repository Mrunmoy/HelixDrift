**Rationale:**

**1. Code is a liability without active use:**
- 50+ lines of `#ifdef CONFIG_HELIX_STAGE4_TDMA_ENABLE` scattered across 1200-line `main.cpp`
- 6 global variables (`stage4_state_val`, counters) in the hot path
- State machine logic (FREE → CANDIDATE → LOCKED) interleaved with midpoint estimator
- Kconfig menu with 5 tunable parameters (cycle,● Read (Design-reviewer agent — RF architecture review)
  └ Running

 slot, guard, lock_N)
- **Zero runtime cost when disabled**, but 100% cognitive load cost for anyone reading the sync path

**2. The design is already well-documented:**
- `docs/RF_STAGE4_EXPLORATORY.md` — Path X1 rationale
- `docs/RF.md` §7.12–§7.14 — v19/v20 implementation + v21 A/B results
- `docs/RF_V20_FINDINGS.md` — failure analysis (FIFO=1 discovery)
- Kconfig comments explain every parameter
- The code itself is clean and well-commented

**If TDMA is ever needed again, a future engineer has everything required to re-implement from scratch** (or resurrect from Git history). The current code is *not* a reusable module — it's tightly coupled to the now-obsolete "FIFO=8 + TDMA" architecture.

**3. Git history preserves the artifact:**
- Stage 4 lived in production (v20) for one deployment cycle
- Full measurement data in `docs/RF_V20_FINDINGS.md`
- Commit `abc1234` (lookup actual SHA) contains the implementation
- Tag the commit with `stage4-tdma-path-x1-reference` before deleting

**4. The FIFO=1 architecture is fundamentally different:**
- Stage 4 assumed "FIFO depth causes bias, TDMA reducesThe FIFO depth last reviewer is"
- FIFO=1 doing a revealed deeper RF pass than " expected. IFIFO depth causes bias, * already haveelim enough to see the direction, but I’m giving it one more short wait so the top-line recommendation reflects the full “team” rather than two-thirds of it.inating depth* fixes bias"
- TDMA is now solving a problem (

FIFO queuing) that no longer exists
- If you bring FIFO back to depth >1 (e.g., for >15 Tag scaling), the *entire sync architecture* needs rework (hardware timestamps, not midpoint estimation)

**Deletion checklist:**
1. ✅ Remove `#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)` blocks from `main.cpp`
2. ✅ Remove Kconfig `menu "Stage 4 TDMA"` section (keep the comment referencing the docs)
3. ✅ Remove `CONFIG_HELIX_STAGE4_TDMA_ENABLE=n` line from `node.conf` (no longer needed)
4. ✅ Update `docs/RF.md` §9 to say "Stage 4 was deleted in v22 after FIFO=1 superseded it; see Git tag `stage4-reference` or commit SHA for the implementation"
5. ✅ Keep all measurement docs (`RF_V20_FINDINGS.md`, `RF_STAGE4_EXPLORATORY.md`) — they explain *why* it was tried and *why* it was deleted

**Keep checklist:**
- ❌ Do NOT keep dormant code "just in case" — it's technical debt
- ❌ Do NOT keep Kconfig with `default n` — future builders will ask "why is this here?"
- ✅ DO keep the paper trail (docs, Git history, this review)

---

### **Final Recommendation**

**DELETE Stage 4 code in v22. Preserve full documentation and Git history.**

**One-line justification:**
*FIFO=1 fixed the asymmetry problem TDMA was designed to mitigate; v21 A/B showed TDMA adds zero sync value at 28% throughput cost; and future >15-Tag scaling would require a fundamentally different architecture (hardware TX timestamps or per-Tag pipes), not incremental tuning of Path X1.*

**If PC fusion demands <5 ms cross-Tag span:** implement the glitch-reject filter first (trivial, high ROI). If that's insufficient, the next step is **per-Tag ESB pipes** (limits fleet to 8 Tags but eliminates bleed-through), not TDMA resurrection.

**If you need to defend this decision in 6 months:** point to `docs/RF_V20_FINDINGS.md` lines 110–123 ("What Stage 4 DID NOT deliver") and `docs/RF_FIFO1_DISCOVERY.md` lines 104–120 ("Revisit Stage 4 TDMA with this baseline").

---

**Code hygiene impact:**
- −50 lines of conditional compilation
- −6 global variables
- −1 Kconfig menu (5 tunables)
- +0 behavioral change (already disabled in v21)

**Risk:** Zero. The code is already compiled out and has been for one full deployment cycle.

**Effort:** 30 minutes (delete + doc update + commit message linking to this review).● Read (Design-reviewer agent — RF architecture review)
  └ Completed

**Recommendation: delete Stage 4 TDMA from the active tree, keep the docs/history, and pursue the midpoint glitch-reject instead.**

**Embedded RF engineer:** **Delete.** FIFO=1 fixed the real bug: asymmetric RTT from Hub ACK-payload queueing. Stage 4 was a valid experiment, but v20 showed it did **not** isolate airtime enough to change cross-Tag span, and v21 A/B showed FIFO=1 alone gives the sync win without the 28% throughput hit. On **B**: TDMA is mostly **orthogonal** to the fat-tail; the bleed-through mechanism is wrong-anchor acceptance / large midpoint jumps, not primarily lack of slotting. Perfect TDMA might reduce opportunities, but the proposed midpoint glitch-reject is the direct fix.

**Protocol architect:** **Delete, don’t refactor-lite.** The salvageable lesson is the measurement, not the mechanism. The `FREE/CANDIDATE/LOCKED` state machine is interesting, but keeping it without the scheduler creates a misleading half-architecture and doesn’t address the current problem. For **A**: Stage 4 is “nice to learn, delete now,” not a reusable building block. For **C**: a future >10 Tag or multi-Hub scale-up could require scheduling again, but probably not this Path X1 design; if scale breaks FIFO=1 later, you likely want a different architecture entirely (true TX timestamps, beaconing, or per-Tag pipes), not this `k_usleep` slot-align path.

**Product owner:** **Delete for roadmap clarity.** FIFO=1 invalidated Stage 4’s value proposition. Keeping dormant code invites future engineers to re-enable a measured dead end and re-pay the 28% throughput cost. Regret risk in 6 months is **low** because the evidence is documented and the code remains recoverable in git if a new scaling regime appears.

**Maintenance engineer:** **Delete from code, archive in docs.** Current burden is small at runtime but real in comprehension: Kconfig knobs, guarded main-loop logic, and orphan-prone SUMMARY fields. This is exactly the kind of “optional experiment” that ages into confusion. If you insisted on keeping it, it would need a loud **deprecated / future-use-only** banner; I still would not keep it.

**Consensus answers**
1. **A:** Very little is worth salvaging in-tree. Keep the writeups; delete the implementation.
2. **B:** Stage 4 would not have fundamentally fixed the cross-Tag fat-tail; that’s mostly orthogonal to the midpoint-jump / bleed-through problem.
3. **C:** Possible future need exists only in a materially different scale regime; if that happens, you’ll likely want a new design, not this one. Deleting now is unlikely to be regretted.
4. **D: Safe unwind**
   1. Remove `HELIX_STAGE4_*` Kconfig entries.
   2. Remove all `#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)` blocks in `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp`.
   3. Remove the Stage-4 block from `zephyr_apps/nrf52840-mocap-bridge/node.conf`.
   4. Remove Stage-4-only SUMMARY fields (`s4=...`) so no orphan telemetry remains.
   5. Move `docs/RF_STAGE4_EXPLORATORY.md` to `docs/archive/rf/` rather than deleting the history.
   6. Update `docs/RF.md` / decision log to say FIFO=1 superseded Stage 4 and glitch-reject is the next lever.
5. **E:** Not recommended; if kept, it needs an explicit deprecation header, but deletion is cleaner.


Changes   +0 -0
Requests  1 Premium (3m 46s)
Tokens    ↑ 1.7m • ↓ 17.7k • 1.4m (cached) • 6.2k (reasoning)
