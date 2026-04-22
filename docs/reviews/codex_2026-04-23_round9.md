-    // partial in-flight state (bytesReceived() resets to 0).
+    // Write a partial chunk so we can prove the second begin() CLEARS it,
+    // not just resets the mocked erase count. (Copilot review round 8:
+    // test body needs to actually exercise the "clears partial state"
+    // contract, not just restate it in a comment.)
+    const uint8_t chunk[] = {0xAA, 0xBB, 0xCC, 0xDD};
+    EXPECT_CALL(backend, writeChunk(0, chunk, 4)).WillOnce(Return(true));
+    ASSERT_EQ(mgr.writeChunk(0, chunk, 4), OtaStatus::OK);
+    ASSERT_EQ(mgr.bytesReceived(), 4u);
+
+    // Second begin() without an explicit abort() succeeds AND clears the
+    // partial in-flight state (bytesReceived() resets to 0, next offset
+    // expected = 0).
     EXPECT_EQ(mgr.begin(2048, 0), OtaStatus::OK);
     EXPECT_EQ(mgr.state(), OtaState::RECEIVING);
     EXPECT_EQ(mgr.bytesReceived(), 0u);
diff --git a/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp b/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
index ba83962..b803fdc 100644
--- a/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
+++ b/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
@@ -1123,6 +1123,11 @@ static void maybe_send_frame(void)
 	              CONFIG_HELIX_STAGE4_SLOT_US
 	              <= CONFIG_HELIX_STAGE4_CYCLE_US,
 	              "Stage 4: N_Tags * SLOT_US must fit in one CYCLE_US");
+	/* GUARD_US must be strictly smaller than SLOT_US; otherwise every
+	 * TX falls inside its own guard and slot alignment is meaningless.
+	 * Round-8 reviewer nit from Codex + Copilot. */
+	static_assert(CONFIG_HELIX_STAGE4_GUARD_US < CONFIG_HELIX_STAGE4_SLOT_US,
+	              "Stage 4: GUARD_US must be < SLOT_US");
 	/* Stage 4 Path X1: if we're LOCKED, align TX to our TDMA slot.
 	 * Hub-time "now" = Tag local_us - midpoint_offset_us. Slot start
 	 * = (node_id - 1) × SLOT_US in Hub-time. ALWAYS wait for the

codex
CLEAN - no further concerns

No required changes found in the reviewed delta.

A. `git fetch --depth=1 "$url" "$sha"` from a temp repo is the right reachability proof for this CI purpose. It tests whether the remote can serve the exact gitlink commit, including non-tip ancestor commits that `ls-remote` would miss. GitHub supports fetching reachable object IDs by SHA, so this should behave like the submodule update path you are trying to preflight. Main caveat: it will fail for commits not reachable from any remote ref, or for private submodules without usable credentials, which is the desired failure mode here.

B. The round-7 to round-8 follow-ups look resolved:
- `.github/workflows/ci.yml`: now checks every `.gitmodules` path and avoids the prior false-negative `ls-remote` logic.
- `tests/test_ota_manager.cpp`: the test now proves partial in-flight state exists before re-`begin()` and is cleared after.
- `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp`: `GUARD_US < SLOT_US` closes the missing TDMA config invariant.

I did not rerun the full host suite; this was a focused final review of the named files/current `5b5058e` state.
tokens used
34,550
CLEAN - no further concerns

No required changes found in the reviewed delta.

A. `git fetch --depth=1 "$url" "$sha"` from a temp repo is the right reachability proof for this CI purpose. It tests whether the remote can serve the exact gitlink commit, including non-tip ancestor commits that `ls-remote` would miss. GitHub supports fetching reachable object IDs by SHA, so this should behave like the submodule update path you are trying to preflight. Main caveat: it will fail for commits not reachable from any remote ref, or for private submodules without usable credentials, which is the desired failure mode here.

B. The round-7 to round-8 follow-ups look resolved:
- `.github/workflows/ci.yml`: now checks every `.gitmodules` path and avoids the prior false-negative `ls-remote` logic.
- `tests/test_ota_manager.cpp`: the test now proves partial in-flight state exists before re-`begin()` and is cleared after.
- `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp`: `GUARD_US < SLOT_US` closes the missing TDMA config invariant.

I did not rerun the full host suite; this was a focused final review of the named files/current `5b5058e` state.
