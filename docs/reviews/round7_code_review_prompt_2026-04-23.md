Code review for overnight RF work on HelixDrift. All changes live in
a single commit range on branch nrf-xiao-nrf52840. Please review
for bugs, correctness issues, embedded-safety concerns, and
regressions.

Context: this is a 10-Tag nRF52840 mocap system (1 Hub dongle, 10
Tags on ESB pipe 0). See docs/RF.md for the full architecture.
Tonight's work delivered Stage 2 rx_node_id filter, Stage 3
midpoint RTT estimator (v5 anchor), Stage 3.5 midpoint wired into
sync_us, Stage 3.6 tx_ring race fix, Stage 4 Tag-side TDMA slot
scheduling (opt-in, default OFF after Kconfig), and the big win —
Hub ESB TX FIFO depth forced to 1 which cut per-Tag sync error by
95%.

Specifically please scrutinise:

1. zephyr_apps/nrf52840-mocap-bridge/src/main.cpp — the
   node_handle_anchor() midpoint computation, the tx_ring
   push/pop under ISR, the Stage 4 state machine and slot-aligned
   k_usleep in maybe_send_frame. All concurrency is single-core
   nRF52 single-ESB-ISR, so no locking but ordering matters (DMB
   used where required).

2. zephyr_apps/nrf52840-mocap-bridge/Kconfig — Stage 4 config
   options.

3. zephyr_apps/nrf52840-mocap-bridge/central.conf — only new line
   is CONFIG_ESB_TX_FIFO_SIZE=1.

4. tools/nrf/fleet_ota.sh — now pre-nukes stale build artifacts
   before rebuild. Does this correctly handle the case where build
   fails partway?

Key risk areas I'm worried about:

A. uint32 wraparound in midpoint math. The tag_mid and hub_mid
   arithmetic relies on modular subtraction. On an underflow, the
   signed int32 cast can produce a correctly-interpreted offset —
   but only if the clocks aren't more than 2^31 microseconds apart
   (~35 min). Tag reboot vs Hub uptime can breach that if Hub runs
   for > 1 hour without a Tag reboot. Is there a defensive check I
   should add?

B. Stage 4 slot-aligned TX in maybe_send_frame uses k_usleep up to
   20 ms inside the main loop. This blocks the Tag main thread for
   up to a full cycle. OTA event handling (atomic_cas on
   ota_reboot_pending) happens BEFORE maybe_send_frame in main
   loop so OTA reboot latency is unchanged. Any other main-loop
   work that would suffer?

C. fleet_ota.sh — I added `rm -f` of merged.hex and
   zephyr.signed.bin before build. If the user Ctrl+C's during
   build, they'd be left with no deployable artefact. Acceptable
   risk?

D. CI failure: external/SensorFusion submodule pointer was a
   commit only local, not on origin. I pushed the missing commits
   to SensorFusion origin/main. Is there a better long-term fix to
   prevent this recurrence?

Constructive feedback welcome on code style, readability, or any
embedded pitfalls I may have missed. Keep reply focused — under
500 words per reviewer.

Diffstat for the changes:
 tools/nrf/flash_tag.sh                         |   2 +-
 tools/nrf/fleet_ota.sh                         |  11 +-
 zephyr_apps/nrf52840-mocap-bridge/Kconfig      |  51 ++
 zephyr_apps/nrf52840-mocap-bridge/central.conf |   9 +
 zephyr_apps/nrf52840-mocap-bridge/node.conf    |  11 +-
 zephyr_apps/nrf52840-mocap-bridge/src/main.cpp | 395 +++++++++++-
