Round 9 final review. This is round 3 of 3 per user directive.

In commit 5b5058e ("fix(ci+test): round 8 reviewer follow-ups") I
addressed your round-8 findings:

1. CI submodule reachability check now uses `git fetch --depth=1
   <url> <sha>` in a temp repo (replacing the flawed `ls-remote`
   approach that would have produced false positives for non-tip
   ancestor commits). Also iterates ALL .gitmodules entries, not
   just external/SensorFusion.

2. OtaManager test body strengthened: now writes a chunk before
   the second begin() and asserts bytesReceived()==4 BEFORE the
   re-begin, then ==0 AFTER. Actually exercises the "clears
   partial in-flight state" contract.

3. Added static_assert(GUARD_US < SLOT_US) alongside the existing
   (N_Tags * SLOT_US <= CYCLE_US) invariant.

Local: 299/299 host tests pass. CI: pending (just pushed).

Please re-review the files:
- .github/workflows/ci.yml (the new verify-reachability step)
- tests/test_ota_manager.cpp (the strengthened test)
- zephyr_apps/nrf52840-mocap-bridge/src/main.cpp (around the
  static_asserts near line 1100)

Two things specifically:

A. Does the `git fetch --depth=1 "$url" "$sha"` via a temp repo
   correctly prove SHA reachability? Any pitfalls (e.g., does
   GitHub allow fetch-by-sha without allowAnySHA1InWant? I
   believe it does, but want confirmation.)

B. Any remaining concern on the overall round-7→8 delta? If you
   have no further findings, please explicitly reply
   "CLEAN - no further concerns" so we can call this sprint's
   review cycle done.

Under 300 words per reviewer. User is asking for a definitive
sign-off or a final list of required changes.
