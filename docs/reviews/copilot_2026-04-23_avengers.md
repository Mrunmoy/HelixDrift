**Honest assessment of current hardware**  
You can probably improve nRF52 further with:
1. hardware TX timestamping
2. real Hub beaconing
3. strict TDMA

But even then, **current hardware is a research bridge, not the product architecture**. I would estimate **2–4 weeks** to prove an **8-Tag @ 200 Hz single-channel TDMA** prototype on nRF52, and **another 4–8 weeks** for a **2-radio base station**. I would **not** fund more ESB-only work except as a control experiment.

**Pragmatic migration path**
- Keep current app/sensor/fusion layers.
- Freeze ESB as the reference path.
- Build a **new transport layer beside it**, not inside it:
  - common time API
  - packet scheduler
  - hardware timestamp driver
  - host merger by master timestamp
- Reuse OTA over BLE if convenient, but keep it off the realtime path.

This is **not** a firmware blow-up. It is a **radio/MAC replacement under stable application code**.

## Hardware

**Per-Tag BOM target $30–50**  
Achievable with Nordic-class MCU + IMU stack + battery + PCB + enclosure. UWB is possible but tightens the budget fast. Wi-Fi is easy on bandwidth, poor on power and wearable simplicity.

**Battery target 4+ hours**  
Feasible with low-duty-cycle proprietary radio + local fusion + BLE-only-on-demand. Much harder with Wi‑Fi or always-on dual-radio Tag designs.

**Form factor**  
Still compatible with your current Tag size class. The real hardware change is more likely at the **Hub/base station**, not the Tag.

## Product

**What I would ship**
- **Tags:** Nordic low-power SoC, local high-rate sensing (500–1000 Hz internal), **200 Hz live transport**
- **Hub:** **2 synchronized radios/channels** on one board, one shared clock, USB to host
- **Realtime link:** custom proprietary **TDMA uplink**
- **OTA / provisioning:** BLE
- **Goal:** <50 ms host latency, **sub-ms cross-Tag p99**, 17 Tags standard

## Disagreements surfaced

- **Conservative view:** nRF52 can still prove the architecture if you abandon ESB and add timestamped TDMA. I agree **for prototyping**.
- **No-compromise view:** if “commercial-suit tier” and “17 Tags” are real, **single-radio anything is the wrong product architecture**. I agree **for shipping**.
- **Hardest disagreement:** **500 Hz live per Tag**. I do **not** recommend committing to that as a shipped baseline on a single low-power radio design. Sense locally at 500 Hz, stream 200 Hz live, and add burst modes only if measurement proves you need more.

**Bottom line:** **ESB is a dead end for the full-product target.** The minimum-risk path is **parallel radio-stack development**: keep HelixDrift’s sensing/fusion/application layers, replace the realtime transport with **hardware-timestamped multi-channel TDMA**, and move to a **2-radio Hub architecture** as the actual product foundation.

I now have sufficient depth across the repo. Let me synthesize the full recommendation.

---

# HelixDrift Architecture Assessment — Commercial-Tier Full-Body Mocap Suit

**Reviewer:** Design Architect  
**Evidence base:** docs/RF.md (complete), RF_V18–V22 findings, RF_CROSS_TAG_SPAN_INVESTIGATION, RF_DRIFT_HYPOTHESIS_REFUTED, RF_MORNING/OVERNIGHT_2026-04-23, SPRINT7–8 redirect docs, hardware-futures.md, rf-sync-architecture.md, rf-protocol-comparison.md, NRF52_SELECTION.md, pose-inference-recommendation.md, firmware/platform structure, simulators tree.

---

## 1. Diagnosis: The Architectural Ceiling You've Already Hit

The repo documents this with unusual clarity. Every sprint-8 experiment narrows to the same verdict:

| Experiment | Finding | Doc |
|---|---|---|
| FIFO=1 | 95% per-tag bias reduction but cross-tag span p99 stuck at 42–53 ms | RF_FIFO1_DISCOVERY, RF.md §7.13 |
| Stage 4 TDMA Path X1 | Deleted unanimously; 28% throughput loss for zero sync gain | RF.md §9, stage4_fate brainstorm |
| v22 glitch-reject | Cross-tag span p99 WORSENED (+5 ms); Tags 1/3 locked into bad baselines | RF_V22_FINDINGS.md |
| Offline drift replay | PC-side rolling bias correction delivered only 1.5 ms improvement | RF_DRIFT_HYPOTHESIS_REFUTED.md |
| v18 retry instrumentation | Retries not the driver of fat-tail | RF.md §7.11 |

**Root cause, as the repo itself concludes (RF_DRIFT_HYPOTHESIS_REFUTED.md):** the cross-tag span fat-tail is *per-tag single-tag tail events* (retry storms, radio glitches, ISR jitter from software timestamps), not drift, not single-sample outliers, not bias accumulation. These are irreducible on shared-pipe ESB with software timestamps. No firmware estimator trick can suppress them.

**The commercial requirements make this worse, not better:**

| Metric | Current (10 tags, 50 Hz) | Commercial target (17-19 tags, 200 Hz) |
|---|---|---|
| Total frames/sec | 500 Hz aggregate | 3,400–3,800 Hz aggregate |
| Sync method | ACK-payload midpoint | Must change (see §3) |
| Cross-tag span p99 | 42–53 ms | **< 1 ms** (50–53× tighter) |
| Per-tag rate | 43–49 Hz | 200 Hz minimum |
| Radio physics headroom | 86% utilized | Would exceed 100% with current approach |

---

## 2. Radio Physics — The Hard Constraint That Kills Shared-Pipe ESB at Scale

A critical calculation the repo hasn't yet done:

**At 200 Hz × 17 tags, bidirectional ESB (current approach):**
- Each bidirectional exchange: ~165 µs TX + 150 µs TIFS + 96 µs ACK + 50 µs guard = ~461 µs
- 17 tags × 461 µs = **7,837 µs per 5 ms superframe = 157% duty cycle**
- **Physically impossible.** Even with zero collisions and zero retries.

**At 200 Hz × 17 tags, unidirectional TDMA (no-ACK data TX, broadcast beacon sync):**
- 200 µs hub beacon + 17 × 200 µs slots = **3,600 µs per 5 ms superframe = 72% duty cycle**
- **Feasible.** Leaves ~1.4 ms slack per superframe for guard intervals and beacon expansion.

At 500 Hz × 17 tags, unidirectional TDMA:
- 2 ms superframe: 200 µs beacon + 17 × 103 µs slots = **1,951 µs per 2 ms = 97.6% duty cycle**
- **At the physical limit.** No margin for guard intervals. A single collision kills a slot. Requires minimal packet size (14–16 bytes, pure quaternion + sequence + HW timestamp).

**Go/No-Go on 500 Hz:**  
`COMMENT — achievable only with packet size ≤16 bytes, zero retries, and sub-µs slot jitter (requires hardware TIMER scheduling on tag). Plan for 200 Hz as primary, treat 500 Hz as stretch requiring additional optimization sprint.`

---

## 3. Sync Authority — What Must Change

**Current approach:** ACK-payload piggybacking. Hub builds a sync anchor when each frame arrives and attempts to return it in the ACK. Stage 2 filter (rx_node_id) and Stage 3 midpoint RTT correction are compensations for the FIFO cross-contamination problem that is inherent to this topology.

**This approach must be abandoned** for commercial tier. Not because it's poorly implemented (it's impressively well-implemented), but because:
- The FIFO cross-contamination problem is architectural: solved only by reducing FIFO to 1 at 28% throughput cost
- At 200 Hz per tag, there is no room for bidirectional ACK exchanges — the air is full of data already
- Sub-ms sync p99 requires hardware-captured timestamps, not ISR-read timestamps

**Recommended sync architecture (validated as correct by both reviewers in RF.md §9):**

```
Hub → Beacon pipe (ESB pipe 1, broadcast address) every 5 ms superframe
  Beacon payload: hub_hw_ts (TIMER CC capture @ RADIO.EVENTS_TXREADY)
  All 17-19 tags RX simultaneously

Tag data TX: ESB pipe 0, TDMA slot, NO ACK
  Payload: node_id + tag_hw_ts (TIMER CC capture @ RADIO.EVENTS_TXREADY) + quaternion + sequence

Hub RX: PPI → TIMER CC capture @ RADIO.EVENTS_END = hub_rx_hw_ts
  Hub outputs to PC: {node_id, tag_hw_ts, hub_rx_hw_ts, quaternion, seq}

PC clock authority:
  offset_tag[i] = hub_rx_hw_ts[i] - tag_hw_ts[i] - propagation_ns
  (propagation ≈ 1 ns/cm × ~30 cm body = 1 ns — negligible)
  PC aligns all tag streams to hub_rx_hw_ts as ground truth
```

**Why this achieves sub-ms cross-tag sync p99:**
- ISR jitter eliminated: timestamps captured by PPI hardware, not read in ISR
- Retry storms eliminated: no-ACK means no retries in data path. Frame either arrives or drops.
- Cross-contamination eliminated: TDMA ensures only one tag TXes per slot
- Hub-authoritative: PC uses hub_rx_hw_ts for all ordering decisions; tag_hw_ts is used only for tag-side drift estimation
- Clock skew between tags: irrelevant — each tag's offset to hub clock is independently measured per-frame

Expected cross-tag span p99 after this change: **< 200 µs** (limited by hardware TIMER resolution ~1 µs, clock stability ~50 ppm, and slot scheduling jitter which is bounded by hardware TIMER).

---

## 4. Should nRF52840 Be Kept or Discarded?

**Decision: KEEP nRF52840 as production stepping stone. Revisit at 500 Hz.**

Evidence-based rationale:

| Factor | Verdict |
|---|---|
| **CPU headroom at 200 Hz** | 5 ms per sample. Mahony AHRS ~100 µs, I2C sensor read ~300 µs, radio TX ~200 µs, total ~600 µs. Easily fits 5 ms. ✅ |
| **CPU headroom at 500 Hz** | 2 ms per sample. With DMA sensor reads and -O2 compiled Mahony, achievable but tight. Measure before deciding. ⚠️ |
| **RAM budget** | 256 KB RAM. Current firmware uses far less. Beacon RX buffer (5 ms × 17 beacons ≈ 2 KB), tx_ring (16 slots × 8 B = 128 B), quaternion FIFO. Fine. ✅ |
| **Flash budget** | 1 MB Flash. Current firmware well within. ✅ |
| **ESB library supports no-ACK + PPI** | Yes — NCS ESB has `CONFIG_ESB_NEVER_DISABLE_TX` and pipe-level ACK control. PPI + TIMER is a standard nRF52 pattern. ✅ |
| **OTA path** | Hub-relay BLE OTA proven 100% reliable on this fleet (docs/NRF_HUB_RELAY_OTA.md). Do not discard. ✅ |
| **BOM at 1k units** | nRF52840 $4.00. Full tag BOM (nRF52840 + LSM6DSO + BMM350 + LPS22DF + battery + PCB) = ~$15-18 at 1k. Well within $30–50 target. ✅ |
| **vs nRF5340 (dual-core)** | +$1–1.50 BOM, provides app/net core isolation for radio stack. Meaningful at 500 Hz to prevent radio ISRs from interfering with fusion loop. Consider if 500 Hz is mandated. ⚠️ |

**For 200 Hz minimum (mandatory): nRF52840 is adequate. GO.**  
**For 500 Hz nice-to-have: try nRF52840 first; migrate to nRF5340 only if radio ISR jitter measurably disrupts fusion cadence at that rate. Plan for this as a no-re-layout upgrade (nRF5340 available in same QFN packages as nRF52840).**

**IMU selection for commercial tier:**  
`REQUEST_CHANGES` — upgrade from LSM6DSO to TDK ICM-42688-P for commercial. Rationale: 2.8 mdps/√Hz vs 4.0 mdps/√Hz gyro noise density; ODR to 32 kHz; -$0.00–+$0.45 delta at volume. At 200–500 Hz the lower noise floor meaningfully improves dynamic tracking at fast motion. The existing sensor driver abstraction in firmware/common cleanly supports driver replacement.

---

## 5. Code / Doc / Asset Reuse vs Rebuild

### KEEP — Zero Modification Needed

| Asset | Location | Why keep |
|---|---|---|
| Hub-relay BLE OTA path | firmware + NRF_HUB_RELAY_OTA.md | Proven 100% reliable; "don't touch" directive in RF.md §5.1 |
| SensorFusion AHRS (Mahony) | external/SensorFusion submodule | Static accuracy <1°, proven across M1–M2, convention bugs fixed |
| Sensor drivers | firmware/common (LSM6DSO, BMM350, LPS22DF) | Proven stable |
| Flash-provisioned node_id scheme | firmware tag code | Clean pattern; survives OTA |
| Python analysis tooling | tools/analysis/ | capture_mocap_bridge_robust.py, sync_error_analysis.py, find_span_outliers.py, offline_bias_replay.py — all immediately applicable to validating new architecture |
| fleet_ota.sh | tools/nrf/ | Production-hardened; fixed silent build failure detection |
| Host simulation harness (M1–M3) | simulators/ | VirtualMocapNodeHarness, 271 C++ + 13 Python tests — usable for validating new sync algorithm |
| Wire format packet structure (v1–v5) | RF_PROTOCOL.md | Data fields reusable; timestamp fields will expand to include hw_ts |
| SUMMARY telemetry infrastructure | firmware hub + tags | Low cost to preserve; valuable for fleet health monitoring |
| MCUboot OVERWRITE_ONLY config | firmware | Stable; swap mode deferred as documented |

### REBUILD — Core RF/Sync Stack

| Asset | Decision | Rationale |
|---|---|---|
| Tag TX loop (`maybe_send_frame`, free-running `k_sleep`) | **Rebuild** | Replace with TDMA slot-timed TX using `k_timer` + PPI TIMER. New design: arm TIMER to fire at slot boundary, PPI captures TX timestamp automatically |
| Hub anchor generation + ESB ACK-payload injection | **Rebuild** | Replace with Hub beacon broadcast on pipe 1. Hub TIMER fires every 5 ms, triggers beacon TX, PPI captures TX timestamp for beacon payload |
| Stage 2/3 sync estimator (`node_handle_anchor`, `midpoint_offset_us`) | **Rebuild** | The midpoint math was a brilliant compensation for a flawed ACK-payload architecture. In the new design, PC computes offsets from (tag_hw_ts, hub_rx_hw_ts) pairs. Tag-side estimator is simplified to: maintain local clock drift model only, not absolute sync. |
| Hub ACK-payload FIFO handling (`esb_write_payload` in PRX interrupt) | **Delete** | No ACK payloads in new design. Hub ISR: read TIMER CC (hardware RX timestamp), read frame payload, write to USB FIFO. Done. |
| `CONFIG_ESB_TX_FIFO_SIZE` workaround | **Moot** | FIFO=1 was a compensation for ACK-payload architecture.
Changes   +0 -0
Requests  1 Premium (12m 57s)
Tokens    ↑ 1.3m • ↓ 17.8k • 962.1k (cached) • 6.8k (reasoning)
