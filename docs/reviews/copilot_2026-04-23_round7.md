  ```- **Severity:** MEDIUM  
  **File:** `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:884-896, 906-928`  
  **Description:** `node_handle_anchor()` stores the midpoint offset in `int32_t` and later computes deltas in signed space. If a tag reboots while the hub clock is >~2^31 µs ahead (~35.8 min), the modulo-32-bit midpoint can cross the signed boundary even when the true drift is small. That makes the next `mdiff` appear enormous and can spuriously blow the midpoint-step histogram / Stage 4 lock logic.  
  **Fix:** Keep midpoint state as `uint32_t`, do modular subtraction first, then cast the delta to `int32_t` only for the step value. Add a defensive validity/reset path if the first post-reboot sample is ambiguous.

- **Severity:** LOW  
  **File:** `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:678-687, 969-983`  
  **Description:** The ACK/tx-ring timestamp is pushed before checking whether `esb_write_payload(&ack)` actually succeeds. With `CONFIG_ESB_TX_FIFO_SIZE=1`, enqueue failure becomes expected whenever one ACK is already pending, so the ring can accumulate phantom entries. Later `TX_SUCCESS` pops those stale timestamps and corrupts latency / pending-depth telemetry.  
  **Fix:** Only push the ring entry after successful `esb_write_payload()`, or explicitly roll back the head on failure.The reviewer finished; I’m pulling its findings now and then I’ll turn them into a concise bug-focused review.

● Read (Embedded-code-review agent — Reviewing RF diff)
  └ Completed

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L650:1010 (361 lines read)

● Read Kconfig
  │ zephyr_apps/nrf52840-mocap-bridge/Kconfig
  └ L1:240 (172 lines read)

● Read fleet_ota.sh
  │ tools/nrf/fleet_ota.sh
  └ L1:220 (161 lines read)

● Read central.conf
  │ zephyr_apps/nrf52840-mocap-bridge/central.conf
  └ L1:120 (62 lines read)

● Read node.conf
  │ zephyr_apps/nrf52840-mocap-bridge/node.conf
  └ L1:160 (65 lines read)

I’ve got the reviewer’s two concrete findings. I’m checking the surrounding code and the shell script once so the handoff covers your specific wraparound, sleep, and artifact-risk questions cleanly.

● Search (grep)
  │ "merged\.hex|zephyr\.signed\.bin|build failed|continue"
  │ (tools/nrf/fleet_ota.sh)
  └ 17 lines found

● Search (grep)
  │ "maybe_send_frame|stage4_|k_usleep\(|tx_ring_head|pending_tx_valid|atomic_se
  │ t\(&tx_ready" (zephyr_apps/nrf52840-mocap-bridge/src/main.cpp)
  └ 8 lines found

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L220:360 (141 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L1030:1165 (136 lines read)

● Read copilot-tool-output-1776894008574-r17jpp.txt
  │ /tmp/copilot-tool-output-1776894008574-r17jpp.txt
  └ L1:220 (167 lines read)

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L1160:1205 (45 lines read)

● Search (grep)
  │ "maybe_send_frame\(|ota_reboot_pending|while \(true\)|for \(;;\)|k_msleep"
  │ (zephyr_apps/nrf52840-mocap-bridge/src/main.cpp)
  └ 48 lines found

● Read main.cpp
  │ zephyr_apps/nrf52840-mocap-bridge/src/main.cpp
  └ L1668:1705 (27 lines read)

● Search (grep)
  │ "histo_bucket_us" (zephyr_apps/nrf52840-mocap-bridge/src/main.cpp)
  └ 49 lines found

**Two real correctness issues stood out; the rest looks mechanically sound.**

1. **Medium — midpoint wraparound can misclassify a healthy first sample after long uptime/reboot**  
   `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:880-907`  
   `tag_mid - hub_mid` is stored directly in `int32_t` (`new_midpoint`), then later differenced in signed space. That works only while the true modular delta stays within ±2^31 us. If a Tag reboots while the Hub clock is >~35.8 min away, the modulo midpoint can cross the sign boundary and the next `mdiff` looks huge even if the real offset is fine. That can falsely spike `midpoint_step_bucket` and churn Stage 4 lock/demotion.  
   **Recommendation:** keep midpoint state as `uint32_t`, compute `uint32_t raw = tag_mid - hub_mid`, and only cast the modular delta `(raw - prev_raw)` to `int32_t` when you need a signed step. Also reset/reseed midpoint state on “first sample after reboot / ambiguous sample”.

2. **Low — Hub ACK queue telemetry can go stale when `esb_write_payload()` fails**  
   `zephyr_apps/nrf52840-mocap-bridge/src/main.cpp:675-687, 969-983`  
   The anchor queue timestamp is pushed **before** checking `esb_write_payload(&ack)`. With `CONFIG_ESB_TX_FIFO_SIZE=1`, enqueue failure is no longer rare; when that happens, the ring still contains a phantom timestamp. The next `TX_SUCCESS` pops that stale entry and corrupts `ack_tx_latency_bucket` / pending-depth telemetry.  
   **Recommendation:** only push the queue timestamp after successful `esb_write_payload()`, or roll back `anchor_queue_ring_head` on failure.

**On your explicit risk questions:**

- **B (`k_usleep` in `maybe_send_frame`)**: I don’t see another functional victim in the node main loop. After the OTA reboot check, the loop only does `maybe_send_frame()`, periodic `report_summary()`, then `k_sleep()` (`main.cpp:1678-1690`). ESB RX/TX handling stays ISR-driven, so the sleep mainly delays summary printing and the next loop iteration. It’s still a throughput/latency tradeoff, but not an obvious hidden correctness regression.

- **C (`fleet_ota.sh` pre-nuke on failed/Ctrl+C build)**: acceptable for a **stress-test** script, but not ideal operationally. Right now a partial build leaves no deployable artifact by design (`tools/nrf/fleet_ota.sh:82-92`). Safer pattern: build into a temp path and only replace `zephyr.signed.bin`/`merged.hex` after success.

- **D (submodule SHA only local)**: best long-term fix is an automated guard. Add CI or a pre-push check that the submodule SHA recorded in the superproject is reachable from the submodule remote, e.g. `git -C external/SensorFusion ls-remote origin <sha>` and fail if absent.

I didn’t see a correctness problem in the new Stage 4 Kconfig defaults or in `central.conf` forcing `CONFIG_ESB_TX_FIFO_SIZE=1`; that change matches the intended “drop instead of backlog” behavior.


Changes   +0 -0
Requests  1 Premium (4m 59s)
Tokens    ↑ 774.7k • ↓ 17.3k • 668.8k (cached) • 12.0k (reasoning)
