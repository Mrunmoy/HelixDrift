Round 8 review — follow-up on round 7 fixes.

In commit c573335 ("fix(ci+rf): apply Codex+Copilot review + make
CI pass") I addressed your round 7 findings. CI is now green
(299/299 host tests, nRF smoke passes). Please re-review:

1. zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:675-696 —
   anchor-queue push moved AFTER esb_write_payload() success.
   Is the ordering correct now? Any remaining concern with
   FIFO=1 behaviour?

2. zephyr_apps/nrf52840-mocap-bridge/src/main.cpp, the declaration
   near line 262: midpoint_offset_us is now
   `static volatile int32_t`. The store at line ~909 uses __DMB()
   between `midpoint_offset_us = new_midpoint` and
   `midpoint_offset_valid = 1u`. Readers (maybe_send_frame Stage 4
   path at ~1121 and node_fill_frame at ~724) check valid before
   reading offset. Is the memory model correct on Cortex-M4
   without acquire/release barriers on the read side too, or is
   a reader-side __DMB() also required?

3. main.cpp ~1100: static_assert for
   `MAX_TRACKED_NODES * SLOT_US <= CYCLE_US`. Is that the right
   invariant? Should we also assert on LOCK_N sanity?

4. tools/nrf/fleet_ota.sh: pre-rename artifacts to .last-good.
   If the round succeeds, `.last-good` is from THAT round's
   previous output (one OTA cycle back). Is that operationally
   useful, or should we keep a chain of last-3? Over-engineering?

5. .github/workflows/ci.yml: new "Verify submodule SHAs are
   reachable on remote" step. The bash is a bit dense. Any obvious
   bug? It's meant to fail with a helpful error if the superproject
   references a commit that isn't on the submodule's remote.

6. tests/test_ota_manager.cpp: renamed the stale test to
   BeginWhileReceivingImplicitlyAbortsAndRestarts and rewrote
   assertions to match the intentional re-init behaviour that
   commit 4c53235 introduced. Does the test name and body
   correctly capture the current contract?

Anything else you noticed on a second pass that you didn't
surface in round 7? Keep reply under 400 words per reviewer.

If you have no new findings, please explicitly say "CLEAN - no
further concerns" so I can stop iterating.
