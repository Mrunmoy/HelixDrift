# RF next steps — design brief (for group review)

**Branch:** `nrf-xiao-nrf52840`
**Date:** 2026-04-21
**Status:** decision pending — user asked for a group review before
picking a direction.

Reviewers: @Copilot, @Codex. Please each spin up your own sub-teams /
sub-agents (protocol person, embedded RF person, hardware person,
product person, whatever you think fits) and come back with a
consensus recommendation. Feel free to challenge any assumption
below — especially the "is it rock-solid?" claim in the current-state
section.

---

## 1. Product context (what we're actually doing)

HelixDrift is a body-worn motion-capture system:

- **10 Tags** (nRF52840 ProPico boards) at body joints. Each streams
  orientation + position frames at 50 Hz on ESB to a single Hub.
  `CONFIG_HELIX_MOCAP_SEND_PERIOD_MS = 20`.
- **1 Hub** (nRF52840 dongle). Central receiver. Forwards frames
  to a PC over USB CDC. Same Hub also relays BLE OTA to Tags when
  commanded.
- **Radio:** ESB PRX/PTX on pipe 0, shared address for all 10 Tags.
  2 Mbps, DPL mode, selective auto-ACK, `retransmit_count=10`,
  per-Tag spread `retransmit_delay=600+50*node_id µs`. Sync anchor
  piggybacks on the ACK payload (current wire is anchor v3, 10 B,
  carries `OTA_REQ` flag + `ota_target_node_id` for BLE OTA
  targeting — see `docs/NRF_HUB_RELAY_OTA.md` and
  `docs/RF_PROTOCOL_REFERENCE.md`).
- **Target mocap rate:** 50 Hz per Tag. PC reads one mocap frame per
  Tag ~every 20 ms.

## 2. Where we are (measured, verified)

### 2.1 OTA path — rock-solid

- Hub-relay OTA validated 20/20 on 10-Tag fleet at commit `b112c2f`
  (tagged `ota-reliable-v1`).
- Single firmware binary across the fleet; each Tag's `node_id` is
  flash-provisioned at `0xFE000`.
- Failure modes fully characterised and committed as idle-connection
  + session-ceiling guards (see `docs/OTA_CODE_REVIEW_REPORT.md`).
- ~240 s per sequential Tag OTA; 40 min to update the full fleet.

### 2.2 RF throughput — good, improved +16 % by Phase C

Baseline (v11, pre-Phase-C, 9 healthy Tags, 10 min capture):
combined 399 Hz of 500 nominal, per-Tag mean 41.8 Hz.

After Phase C (staggered retransmit_delay + retransmit_count 6→10 +
200 µs × node_id boot-time TX offset):
combined 436 Hz, per-Tag mean 48.5 Hz (range 45.5-51.2 Hz).

### 2.3 Reset-recovery — clean

Hub mid-stream reset → each Tag sees ~2 s gap then resumes. Tested
4 consecutive Hub resets with no cumulative degradation.

### 2.4 Sync error — the ambiguous one

Old metric claimed p99 cross-Tag span of 2.1 s (baseline) / 122 s
(post-Phase-C). Investigation revealed that was a **measurement
artefact** (running max-min over stalest last-seen sync_us — one dead
Tag broke it).

Real metric (`tools/analysis/sync_error_analysis.py`, bin-based, Tag 5
excluded as it's hardware-flatlined):

| Metric | p50 | p99 | Max |
|---|---:|---:|---:|
| Cross-Tag span (50 ms bins, ≥5 Tags) | **19 ms** | **50 ms** | 64 ms |
| Per-Tag `|sync - rx|` (example) | 13 ms | 22 ms | 30 ms |

Per-Tag mean **systematic bias: -14 ms**, uniform across 9 Tags.
Believed to be the ESB ACK-payload TIFS race: Hub's
`esb_write_payload` call misses the ~150 µs TIFS window on ~70 % of
frames, pushing the anchor onto the Tag's next frame's ACK → adds the
20 ms TX period as latency.

### 2.5 Known outstanding

- **Tag 5** (HTag-0126) hardware-dead post-Phase-C. Suspected
  charge-circuit / USB-power fault on that specific board.
  Requires physical inspection — not a firmware issue.
- **Anchor v3** does not carry Hub's anchor-TX timestamp, so the
  current single-sample offset estimator bakes the ACK-path latency
  into every offset calculation.

## 3. Candidate next tasks (the question to debate)

### 3.1 Anchor v4 — `anchor_tx_us` field, midpoint offset estimator

**Gain:** eliminates the 14 ms systematic bias. Likely brings p99
cross-Tag span from 50 ms → <10 ms (hardware ACK-jitter is single-µs).

**Mechanism:** add `uint32_t anchor_tx_us` field to `HelixSyncAnchor`
(grows to 14 B). Hub populates at anchor TX. Tag records `local_at_tx`
in `maybe_send_frame` and computes
`offset = (local_at_tx + local_at_anchor_rx)/2 - anchor_tx_us`.

**Cost:** ~half a day. Wire-format change with same migration story
as v2→v3 (one OTA round for the fleet).

**Risk:** straightforward protocol extension; the Zephyr/NCS radio API
exposes TX-done events so `anchor_tx_us` can be timestamped accurately.

**When it matters:** the moment you actually deploy to capture tight
motion (e.g. dance, martial arts). For body-mocap at 50 Hz, 50 ms p99
is ~2.5 frames of uncertainty — workable but not pretty.

### 3.2 Phase E — per-Hub ESB pipe derivation

**Problem it solves:** today both Hubs (if two were in one room) would
RX each other's Tags and cross-talk. `CLAUDE.md` calls for FICR-derived
per-Hub base address.

**Option A (flash-provisioned):** Tag has Hub's base address at
`0xFE004` (next word after `node_id`). Extend SWD provisioning,
read at boot, use instead of hard-coded constant. **Cost ~2 h.**

**Option B (discovery broadcast):** Hub broadcasts its FICR-derived
address on a well-known "rendezvous" address+channel at boot. Tags
listen, capture, switch. **Cost 1-2 days, non-trivial protocol.**

**Option C (hybrid):** A as default, B as fallback if Tag's
provisioned Hub is silent for N seconds. **Cost 2+ days.**

**When it matters:** when you actually deploy >1 Hub in the same
room (multi-user mocap). Not a current blocker.

### 3.3 Single-Tag reset / rejoin characterisation

**Gain:** confirm Tag-side recovery path works cleanly.

**Mechanism:** existing harness, reset Tag 1 (only SWD-accessible Tag)
mid-stream, measure Tag 1's dropout + rejoin while other 9 Tags keep
streaming.

**Cost:** ~0.5 h. Low risk.

**When it matters:** if operations ever include mid-stream Tag
reboots. Today probably rare.

### 3.4 Parallel OTA (currently parked as task #21)

**Cheap path:** bump `CONFIG_BT_MAX_CONN` to 4, multiplex BLE OTA
across 4 simultaneous Tag connections. 40 min fleet update → ~10 min.

**Expensive path:** multiple Hubs working in parallel. Tied to
Phase E.

**Cost (cheap):** ~half a day. **Cost (expensive):** 2+ days after
Phase E.

**When it matters:** when fleet-update time becomes a deployment pain.
Currently once per firmware roll, ~40 min. Not critical.

## 4. The decision I'd pick (writer's note)

My gut: **none of these are today-critical**. The system is
rock-solid for what it's doing now. The right thing is probably to
**stop polishing the RF and move the project forward on the actual
mocap application** — wire the PC-side ingest, fusion, and rendering,
then come back here only when a specific measured number falls short
(if "sync p99 50 ms" breaks something real → do anchor v4; if
"fleet-update 40 min is a pain" → do parallel OTA; etc.).

But I'm explicitly not confident about this conclusion. Debate it.

## 5. Questions for the group

1. Is "19 ms median / 50 ms p99 cross-Tag span" actually acceptable for
   50 Hz mocap? What does the literature / your experience say for
   body-mocap specifically? Golfers vs. dancers?
2. Is the -14 ms systematic bias worth eliminating for its own sake
   (predictability of downstream fusion) or only worth eliminating if
   the final span number isn't good enough?
3. Are we missing a failure mode we haven't tested yet? Long-duration
   (hours)? Thermal? Battery sag on Tags? 2.4 GHz WiFi / BT headphones
   in the same room?
4. For Phase E, should Hub-address discovery use a BLE-level protocol
   (since Tags already speak BLE for OTA) instead of an ESB-level
   rendezvous?
5. If the right answer really is "stop polishing, do the app", what's
   the cheapest RF instrumentation we leave behind so future debugging
   is painless?

Please debate, reach a consensus, and name your top-3 with rationale.

— `docs/RF_NEXT_STEPS_DESIGN_BRIEF.md` — end brief
