
I’ve got the touched files; I’m pulling the exact RF-sync and Stage 4 sections now to answer the ordering/barrier questions precisely.

● Search (grep)
  │ "LOCK_N|MAX_TRACKED_NODES|SLOT_US|CYCLE_US"
  │ (zephyr_apps/nrf52840-mocap-bridge/src/main.cpp)
  └ 13 lines found

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L880:935 (56 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L1080:1160 (81 lines read)

● Show commit diff excerpt (shell)
  │ git --no-pager show c573335 --
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp tools/nrf/fleet_ota.sh
  │ .github/workflows/ci.yml tests/test_ota_manager.cpp | sed -n '1,260p'
  └ 261 lines...

The anchor-queue fix looks directionally right; I’m checking the consume side and the exact publish/read pattern now for any remaining edge cases.

● Search (grep)
  │ "anchor_queue_ring_|ack_tx_dropped_no_queue|pending_tx_valid|midpoint_offset
  │ _valid" (zephyr_apps/nrf52840-mocap-bridge/src/main.cpp)
  └ 25 lines found

● Read OtaManager.cpp
  │ firmware/common/ota/OtaManager.cpp
  └ L1:220 (89 lines read)

● Search (grep)
  │ "begin\(|OtaManager::begin|ERROR_INVALID_STATE|implicit ABORT|abort\(" in
  │ **/*.{cpp,hpp}
  └ 78 lines found

● Show previous main.cpp snippets (shell)
  │ git --no-pager show c573335^:zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  │ | sed -n '640,710p;890,930p;1110,1155p'
  └ 158 lines...

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L972:1025 (54 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L1170:1205 (36 lines read)

● Read OtaManager.hpp
  │ firmware/common/ota/OtaManager.hpp
  └ L28:60 (33 lines read)

● Inspect CI script and gitmodules (shell)
  │ git --no-pager show c573335:.github/workflows/ci.yml | sed -n '24,110p' &&
  │ printf '\n--- gitmodules ---\n' && git --no-pager show c573335:.gitmodules
  └ 97 lines...

I found the consume side and the OTA manager contract. I’m checking the Kconfig bounds for `LOCK_N` before I call the Stage 4 invariant “done.”

● Search (grep)
  │ "Stage 4|LOCK_N|SLOT_US|CYCLE_US" (docs)
  └ 105 lines found

● Search (grep)
  │ "config HELIX_STAGE4_LOCK_N|CONFIG_HELIX_STAGE4_LOCK_N|config
  │ HELIX_STAGE4_SLOT_US|config HELIX_STAGE4_CYCLE_US|config
  │ HELIX_MOCAP_MAX_TRACKED_NODES" in **/Kconfig*
  └ 4 lines found

● Read Kconfig
  │ zephyr_apps/nrf52840-mocap-bridge/Kconfig
  └ L40:175 (133 lines read)

● Read copilot_2026-04-23_round7.md
  │ docs/reviews/copilot_2026-04-23_round7.md
  └ L1:140 (100 lines read)

**One low concern remains; the RF/CI fixes themselves look sound.**

1. **Anchor queue ordering:** yes, this is the right order now. With `CONFIG_ESB_TX_FIFO_SIZE=1`, `esb_write_payload()` failure is part of the design, so pushing only on success is exactly what keeps the FIFO telemetry honest. I don’t see a remaining FIFO=1 bookkeeping bug in that path.

2. **`midpoint_offset_us` publish/read:** on nRF52840 / Cortex-M4, I do **not** think a reader-side `__DMB()` is required for this pattern. The writer-side `__DMB()` plus `volatile` loads/stores is enough for the intended “publish offset, then mark valid” contract on this single-core MCU. Nuance: after the first publish, `midpoint_offset_valid` stays sticky, so readers may race and see either the old or new **whole** 32-bit offset, but not a torn value; that seems acceptable here. If you ever want language-level rigor, use atomics rather than more ad hoc barriers.

3. **Stage 4 invariant:** `MAX_TRACKED_NODES * SLOT_US <= CYCLE_US` is the right core packing invariant. I would **not** add a `LOCK_N` assert; Kconfig already constrains it to `1..50`. If you want one more static sanity check, `GUARD_US <= SLOT_US` is more relevant than `LOCK_N`.

4. **`.last-good` retention:** current one-deep backup is operationally useful and proportionate. Keeping last-3 feels like over-engineering for a stress script unless you’re deliberately using it as a mini artifact archive.

5. **CI submodule reachability step:** no obvious correctness bug for the intended `external/SensorFusion` check. The bash is denser than it needs to be, but it should fail usefully. One nit: the step name says **submodule SHAs** plural, but it only checks `external/SensorFusion`; if that’s intentional, fine, otherwise iterate all gitlink entries.

6. **OTA test:** the **name** is right, but the **body is weaker than the name suggests**. It never writes a chunk before the second `begin()`, so it doesn’t actually prove that partial in-flight state gets aborted/reset. I’d add one `writeChunk()` before the second `begin()` and then assert `bytesReceived() == 0`.

So: **not CLEAN** yet, but only because of the test-strength issue in `BeginWhileReceivingImplicitlyAbortsAndRestarts`.


Changes   +0 -0
Requests  1 Premium (1m 59s)
Tokens    ↑ 333.3k • ↓ 6.6k • 289.9k (cached) • 4.4k (reasoning)
