zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:528:		 stage4_tx_skipped
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:679:		const uint32_t head = anchor_queue_ring_head;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:681:		anchor_queue_ring_head = head + 1u;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:682:		const uint32_t pending = (head + 1u) - anchor_queue_ring_tail;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:724:		midpoint_offset_valid ? midpoint_offset_us : estimated_offset_us;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:866:#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:895:				const int64_t mdiff = (int64_t)new_midpoint - (int64_t)midpoint_offset_us;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:900:				 * tail, mid_step_by_retry[3][3] (7+ retries × >=30ms)
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:901:				 * will dominate mid_step_by_retry[0][3] (1 retry ×
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:904:				mid_step_by_retry[r_bucket][m_bucket]++;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:906:			midpoint_offset_us = new_midpoint;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:909:#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:915:				if (stage4_stable_count >= (uint32_t)CONFIG_HELIX_STAGE4_LOCK_N) {
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:938:	const int32_t prev_offset = estimated_offset_us;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:939:	estimated_offset_us = (int32_t)(local_us - anchor->central_timestamp_us);
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:940:	g_helixMocapStatus.estimated_offset_us = estimated_offset_us;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:949:	const int64_t diff = (int64_t)estimated_offset_us - (int64_t)prev_offset;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:971:			const uint32_t tail = anchor_queue_ring_tail;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:972:			const uint32_t head = anchor_queue_ring_head;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:976:				anchor_queue_ring_tail = tail + 1u;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:990:		 * can be bucketed by retry count. See mid_step_by_retry. */
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1103:#if defined(CONFIG_HELIX_STAGE4_TDMA_ENABLE)
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1105:	 * Hub-time "now" = Tag local_us - midpoint_offset_us. Slot start
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1113:		const uint32_t now_hub = now_tag - (uint32_t)midpoint_offset_us;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1114:		const uint32_t cycle_phase = now_hub % (uint32_t)CONFIG_HELIX_STAGE4_CYCLE_US;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1116:			(uint32_t)(g_node_id - 1u) * (uint32_t)CONFIG_HELIX_STAGE4_SLOT_US;
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1120:		if (cycle_phase <= my_slot_start + (uint32_t)CONFIG_HELIX_STAGE4_GUARD_US) {
zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1124:			delay_us = (uint32_t)CONFIG_HELIX_STAGE4_CYCLE_US

exec
/bin/bash -lc 'git diff --check ae3cc97''^..c442eb7' in /home/mrumoy/sandbox/embedded/HelixDrift
 exited 2 in 0ms:
docs/archive/rf/RF_STAGE4_TDMA_DESIGN.md:5: trailing whitespace.
+Parent: [RF_SYNC_DECISION_LOG.md](RF_SYNC_DECISION_LOG.md)  

exec
/bin/bash -lc 'git submodule status && git ls-tree HEAD external/SensorFusion && git -C external/SensorFusion status --short && git -C external/SensorFusion log --oneline -n 5' in /home/mrumoy/sandbox/embedded/HelixDrift
 succeeded in 0ms:
 656b60c06d91c2c6dcbc5b1165ab0b05f72cb9db external/SensorFusion (v1.1.0-2-g656b60c)
 d045415fb752049a729b61d18205eb45d7d826b5 third_party/mcuboot (v2.3.0-73-gd045415f)
160000 commit 656b60c06d91c2c6dcbc5b1165ab0b05f72cb9db	external/SensorFusion
656b60c sensorfusion: align mahony update conventions
214c28a sensorfusion: seed mahony from first sensor sample
91354f1 Add versioned release workflow and tag packaging
afbc7a8 Remove third_party ESP-IDF submodule
e9afb8c Add node health telemetry frame encoding

codex
**Findings**

1. [Medium] [main.cpp:678](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:678) pushes `anchor_queue_ring_head` before checking `esb_write_payload(&ack)`. With `CONFIG_ESB_TX_FIFO_SIZE=1`, full-FIFO failures are expected, so this records phantom queued anchors. Later `TX_SUCCESS` pops the wrong timestamp and `ack_lat`/`pend_max` become misleading. Move the ring push after successful `esb_write_payload()`, or roll it back on error. Also consider storing `anchor.anchor_tx_us`, not `rx_timestamp_us`, if this is queue-latency telemetry.

2. [Medium] [main.cpp:1127](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1127) plus [main.cpp:1690](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1690): Stage 4 can sleep up to a cycle, transmit, then the main loop always sleeps another `SEND_PERIOD_MS`. If the thread wakes just outside the guard, this can skip a whole cycle and reduce throughput. Default-off makes this non-shipping, but if re-enabled, use absolute next-slot scheduling or have `maybe_send_frame()` own the period sleep.

3. [Medium] [main.cpp:906](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:906) writes `midpoint_offset_us` in ISR while [main.cpp:724](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:724) and [main.cpp:1113](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1113) read it in main, but the offset itself is not `volatile`/atomic. On M4 the 32-bit load/store is naturally atomic, but the compiler contract is still weak. Make the offset atomic/volatile and publish `offset` before `valid` with a barrier.

4. [Low] [Kconfig:133](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/Kconfig:133) does not enforce `10 * SLOT_US <= CYCLE_US`, and [main.cpp:1115](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1115) does not guard `g_node_id`. A bad provisioned ID or config can sleep far beyond one TDMA cycle.

On A: I’d avoid signed-offset dependence. Keep the canonical clock offset as `uint32_t offset_mod = tag_mid - hub_mid`; use modular subtraction for `sync_us` and slot phase. Only derive signed deltas for telemetry when comparing against a previous offset with a bounded modular-diff helper.

On C: pre-nuking is correct for preventing stale OTA deployment, but it should preserve `.last-good` for operator recovery.

On D: add CI that verifies submodule SHAs exist on the configured remote before the superproject passes. I did not run builds/tests for this review.
tokens used
83,672
**Findings**

1. [Medium] [main.cpp:678](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:678) pushes `anchor_queue_ring_head` before checking `esb_write_payload(&ack)`. With `CONFIG_ESB_TX_FIFO_SIZE=1`, full-FIFO failures are expected, so this records phantom queued anchors. Later `TX_SUCCESS` pops the wrong timestamp and `ack_lat`/`pend_max` become misleading. Move the ring push after successful `esb_write_payload()`, or roll it back on error. Also consider storing `anchor.anchor_tx_us`, not `rx_timestamp_us`, if this is queue-latency telemetry.

2. [Medium] [main.cpp:1127](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1127) plus [main.cpp:1690](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1690): Stage 4 can sleep up to a cycle, transmit, then the main loop always sleeps another `SEND_PERIOD_MS`. If the thread wakes just outside the guard, this can skip a whole cycle and reduce throughput. Default-off makes this non-shipping, but if re-enabled, use absolute next-slot scheduling or have `maybe_send_frame()` own the period sleep.

3. [Medium] [main.cpp:906](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:906) writes `midpoint_offset_us` in ISR while [main.cpp:724](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:724) and [main.cpp:1113](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1113) read it in main, but the offset itself is not `volatile`/atomic. On M4 the 32-bit load/store is naturally atomic, but the compiler contract is still weak. Make the offset atomic/volatile and publish `offset` before `valid` with a barrier.

4. [Low] [Kconfig:133](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/Kconfig:133) does not enforce `10 * SLOT_US <= CYCLE_US`, and [main.cpp:1115](/home/mrumoy/sandbox/embedded/HelixDrift/zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:1115) does not guard `g_node_id`. A bad provisioned ID or config can sleep far beyond one TDMA cycle.

On A: I’d avoid signed-offset dependence. Keep the canonical clock offset as `uint32_t offset_mod = tag_mid - hub_mid`; use modular subtraction for `sync_us` and slot phase. Only derive signed deltas for telemetry when comparing against a previous offset with a bounded modular-diff helper.

On C: pre-nuking is correct for preventing stale OTA deployment, but it should preserve `.last-good` for operator recovery.

On D: add CI that verifies submodule SHAs exist on the configured remote before the superproject passes. I did not run builds/tests for this review.
