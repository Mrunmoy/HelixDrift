   271	    mgr.abort();
   272	    EXPECT_EQ(mgr.state(), OtaState::IDLE);
   273	    EXPECT_EQ(mgr.bytesReceived(), 0u);
   274	}
   275	
   276	// ---------------------------------------------------------------------------
   277	// Re-use after terminal states
   278	// ---------------------------------------------------------------------------
   279	
   280	TEST_F(OtaManagerTest, CanBeginAgainAfterAbort) {
   281	    expectSuccessfulBegin();
   282	    ASSERT_EQ(mgr.begin(1024, 0), OtaStatus::OK);
   283	    mgr.abort();
   284	
   285	    expectSuccessfulBegin();
   286	    EXPECT_EQ(mgr.begin(512, 0), OtaStatus::OK);
   287	    EXPECT_EQ(mgr.state(), OtaState::RECEIVING);
   288	}
   289	
   290	TEST_F(OtaManagerTest, CanBeginAgainAfterCommit) {
   291	    const uint8_t image[] = {0xDE, 0xAD};
   292	    const uint32_t goodCrc = crc32(image, sizeof(image));
   293	
   294	    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
   295	    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
   296	    ASSERT_EQ(mgr.begin(sizeof(image), goodCrc), OtaStatus::OK);
   297	
   298	    EXPECT_CALL(backend, writeChunk(0, _, sizeof(image))).WillOnce(Return(true));
   299	    ASSERT_EQ(mgr.writeChunk(0, image, sizeof(image)), OtaStatus::OK);
   300	
   301	    EXPECT_CALL(backend, setPendingUpgrade()).WillOnce(Return(true));
   302	    ASSERT_EQ(mgr.commit(), OtaStatus::OK);
   303	
   304	    // Second transfer after a commit should work
   305	    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
   306	    EXPECT_EQ(mgr.begin(256, 0), OtaStatus::OK);
   307	    EXPECT_EQ(mgr.state(), OtaState::RECEIVING);
   308	}

codex
**Finding**

- [`.github/workflows/ci.yml:39`](</home/mrumoy/sandbox/embedded/HelixDrift/.github/workflows/ci.yml:39>) / repeated at `:90`: the submodule reachability check only proves the SHA is advertised by `ls-remote`, usually a branch/tag tip or peeled tag. It does **not** prove an arbitrary submodule commit is reachable from the remote. A perfectly valid submodule pointer to a non-tip ancestor can fail this check even though `git submodule update --init` would succeed. Use a temp repo fetch instead, e.g. `git fetch --depth=1 "$url" "$sha"` and fail on that. Also consider adding the same check before `nrf-smoke` if that job depends on submodule fetches too.

On the specific questions:

1. Anchor queue ordering looks correct now. With FIFO=1, pushing only after `esb_write_payload()` succeeds avoids phantom entries. Remaining caveat only applies if PRX ACK payloads can produce `TX_FAILED`/flush without a matching pop; I did not see evidence of that path here.

2. Cortex-M4/read side: no reader-side `__DMB()` is required for this embedded ISR/main-thread contract. `volatile` keeps compiler order for `valid ? offset : fallback`, 32-bit load/store is atomic, and stale-old vs fresh-new offset is acceptable after valid is already set. For portable ISO C++ threading this would still be a data race, but that is not the model here.

3. `MAX_TRACKED_NODES * SLOT_US <= CYCLE_US` is the right core invariant. `LOCK_N` is already constrained by Kconfig `range 1 50`; an extra assert is fine but redundant. A more useful optional assert would be `GUARD_US < SLOT_US`.

4. `.last-good` one cycle back is operationally useful and not over-engineered. Last-3 is probably unnecessary unless you have a real rollback workflow.

5. See finding above.

6. Test name matches the contract. The body proves second `begin()` succeeds and erases twice; if you want it to prove “clears partial state,” write one chunk before the second begin so `bytesReceived()` was nonzero first.

Not clean due the CI reachability false-positive risk.
tokens used
63,826
**Finding**

- [`.github/workflows/ci.yml:39`](</home/mrumoy/sandbox/embedded/HelixDrift/.github/workflows/ci.yml:39>) / repeated at `:90`: the submodule reachability check only proves the SHA is advertised by `ls-remote`, usually a branch/tag tip or peeled tag. It does **not** prove an arbitrary submodule commit is reachable from the remote. A perfectly valid submodule pointer to a non-tip ancestor can fail this check even though `git submodule update --init` would succeed. Use a temp repo fetch instead, e.g. `git fetch --depth=1 "$url" "$sha"` and fail on that. Also consider adding the same check before `nrf-smoke` if that job depends on submodule fetches too.

On the specific questions:

1. Anchor queue ordering looks correct now. With FIFO=1, pushing only after `esb_write_payload()` succeeds avoids phantom entries. Remaining caveat only applies if PRX ACK payloads can produce `TX_FAILED`/flush without a matching pop; I did not see evidence of that path here.

2. Cortex-M4/read side: no reader-side `__DMB()` is required for this embedded ISR/main-thread contract. `volatile` keeps compiler order for `valid ? offset : fallback`, 32-bit load/store is atomic, and stale-old vs fresh-new offset is acceptable after valid is already set. For portable ISO C++ threading this would still be a data race, but that is not the model here.

3. `MAX_TRACKED_NODES * SLOT_US <= CYCLE_US` is the right core invariant. `LOCK_N` is already constrained by Kconfig `range 1 50`; an extra assert is fine but redundant. A more useful optional assert would be `GUARD_US < SLOT_US`.

4. `.last-good` one cycle back is operationally useful and not over-engineered. Last-3 is probably unnecessary unless you have a real rollback workflow.

5. See finding above.

6. Test name matches the contract. The body proves second `begin()` succeeds and erases twice; if you want it to prove “clears partial state,” write one chunk before the second begin so `bytesReceived()` was nonzero first.

Not clean due the CI reachability false-positive risk.
