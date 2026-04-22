Brainstorm session: Stage 4 TDMA code — delete or keep?

Please assemble your "team of experts" (embedded RF engineer,
protocol architect, product owner, maintenance engineer) and
come back with a consensus recommendation. This is a keep/delete
architectural decision on ~200 lines of tested code.

Context (read docs/RF.md §7.11-7.16 + docs/RF_FIFO1_DISCOVERY.md
+ docs/RF_V20_FINDINGS.md first):

Stage 4 Path X1 (Tag-side TDMA slot scheduling) was implemented,
fleet-deployed (v20), measured. Then the FIFO=1 one-line Hub
config change delivered the same sub-ms per-Tag sync that Stage 4
was meant to deliver, without the 28 % throughput cost Stage 4
introduces via k_usleep.

In v21 A/B, Stage 4 default is `n`. Code compiles cleanly,
compiles out to zero runtime cost. Both branches tested.

Trade-offs to weigh:

1. **Delete entirely.** Arguments:
   - FIFO=1 is the proven architectural fix. Stage 4 is now
     redundant.
   - ~200 lines + Kconfig + state machine + SUMMARY fields
     impose maintenance burden.
   - Future engineers may try to re-enable it and hit the same
     28 % throughput cost.
   - The 10-ms-bias root cause is documented; that's the real
     lesson.

2. **Keep as optional experiment.** Arguments:
   - Code is tested, works, adds Kconfig-gated functionality.
   - If PC fusion needs tighter than 42 ms cross-Tag span p99
     later, TDMA is a lever worth having.
   - Provides a fall-back architecture for environments where
     FIFO=1 is insufficient (e.g., very high Tag count, where
     FIFO=1 starves the fleet because only one anchor serves
     per TIFS window).
   - Code is already-paid: marginal cost to keep.

3. **Refactor lite — keep only the beacon seed.** Arguments:
   - The real learning was the lock state machine (STAGE4_FREE /
     CANDIDATE / LOCKED). That might be useful as a generic
     "sync confidence" indicator even without TDMA slot
     scheduling.
   - Keep the state machine + SUMMARY telemetry, drop the
     `k_usleep` slot-align path.

4. **Something else your experts propose.**

Specific asks:

A. Given the FIFO=1 discovery was a genuine surprise that
   invalidated the whole Stage 4 value proposition, how much of
   Stage 4 is salvageable vs. "nice to learn, delete now"?

B. The cross-Tag span fat-tail investigation (task #49, doc
   `RF_CROSS_TAG_SPAN_INVESTIGATION.md`) proposes a glitch-reject
   on midpoint. Would Stage 4 TDMA's slot isolation have
   fundamentally changed the bleed-through mechanism, or is it
   orthogonal?

C. Bigger picture — is there a future scaling scenario (>10
   Tags, multi-Hub) where TDMA becomes necessary again? If
   deleting now, would we regret it in 6 months?

D. If you recommend DELETE: confirm the safe way to unwind. All
   of:
     - Remove Kconfig entries (HELIX_STAGE4_*)
     - Remove the `#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)`
       guarded blocks in main.cpp
     - Remove node.conf Stage-4 block
     - Remove docs/RF_STAGE4_EXPLORATORY.md (or keep as history
       in docs/archive/rf/)
     - Clean up any SUMMARY-line fields that become orphans

E. If you recommend KEEP: confirm it's clean enough to stay in
   the tree without risk of confusing future contributors.
   Should we add a `HELIX_STAGE4_FOR_FUTURE_USE_ONLY` warning
   header comment?

Format: structured team opinion (with team member perspectives
differentiated if they disagree) + single-sentence recommendation
at the top + under 600 words total.
