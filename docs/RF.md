# RF — status, history, open questions

**Branch:** `nrf-xiao-nrf52840` &nbsp;·&nbsp;
**Fleet:** 10 Tags at v17 + Hub with Stage 3 (v5 anchor) &nbsp;·&nbsp;
**Tip commit:** `74ad5e6` &nbsp;·&nbsp;
**Updated:** 2026-04-22

This is the **single consolidated RF doc**. Any agent or reviewer
should be able to read just this file and have full context.

Related docs (kept separate on purpose):
- [`RF_PROTOCOL.md`](RF_PROTOCOL.md) — wire-format reference (bytes,
  fields, compat).
- [`rf-sync-requirements.md`](rf-sync-requirements.md) — requirements.
- [`NRF_HUB_RELAY_OTA.md`](NRF_HUB_RELAY_OTA.md) — OTA path (solved).
- [`RF_TIME_SYNC_REFERENCE.md`](RF_TIME_SYNC_REFERENCE.md) — time-sync
  fundamentals (conceptual reference).
- [`RF_SYNC_SIMULATION_SPEC.md`](RF_SYNC_SIMULATION_SPEC.md) — sim-layer
  reference.
- `docs/reviews/` — reviewer dialog log (rounds 1–5).

---

## TL;DR

- **Fleet healthy.** All 10 Tags on v17 firmware; 20-min stability
  run shows per-Tag sync |err| p99 13.5–17.5 ms.
- **Hub-relay OTA is solved** — 100 % reliable, ~240 s per Tag.
- **Cross-Tag sync span p99 is ~53 ms; the v1 requirement is < 1 ms**
  — ~50× gap. This is the **open architectural question**: TDMA
  slots or per-Tag pipes are the remaining levers. Both are
  multi-day efforts. See §8 (Open decisions) and §9 (Stage 4
  proposal).
- **Six design stages already landed** (Stages 1, 1', 2, 3, 3.5,
  3.6) — each narrowed the bias / race / jitter. Summary in §7.

---

## 1. Product context

HelixDrift is a body-worn motion-capture system.

- **10 Tags** (nRF52840 ProPico boards) at body joints. Each streams
  orientation + position at 50 Hz over ESB.
- **1 Hub** (nRF52840 dongle). ESB PRX. Forwards frames to PC over
  USB CDC. Same Hub also relays BLE OTA to Tags.
- **Radio:** ESB PRX/PTX on **shared pipe 0**, 2 Mbps, DPL mode,
  selective auto-ACK, `retransmit_count=10`,
  per-Tag `retransmit_delay = 600 + 50·node_id µs`.
- **Sync:** anchor struct piggybacked on each ESB ACK payload.
- **Target:** 50 Hz per-Tag TX, ≤ 1 ms inter-node sync skew for
  fast-motion mocap.

Use cases (user-confirmed, both on road-map):
- **Slow motion** — rehab, VR avatar, film preview. Low peak
  angular velocity; ms-scale sync usable.
- **Fast motion** — sports, dance, martial arts. 50 ms p99 span
  is ~20° joint-angle error at 400 °/s peak. Not usable for a
  fast-motion demo today.

---

## 2. Current state (what's on the fleet)

| Part | Version | Location |
|---|---|---|
| Hub firmware | v5 anchor + Stage 3 midpoint + Stage 3.6 | SWD-flashed, board `nrf52840dongle/nrf52840/bare` |
| Tag firmware | v17 (Stage 3.6 tx_ring ordering fix) | OTA fleet 10/10 PASS |
| Wire format | v5 (16 B anchor) | Tags accept v1–v5 for rolling upgrade |

### 2.1 Measured sync quality (20-min run, 514k frames)

Full 10-Tag fleet, 30-s settle to prune startup artefacts:

| Metric | Value |
|---|---:|
| Per-Tag \|err\| p99 | **13.5 – 17.5 ms** (every Tag) |
| Per-Tag \|err\| max | **28.0 ms** (Tag 8) |
| Fleet mean bias range | -9094 .. -5474 µs (**3.6 ms** spread) |
| Cross-Tag span p50 | 25.5 ms |
| Cross-Tag span p90 | 42.5 ms |
| Cross-Tag span p99 | **53.5 ms** |
| Cross-Tag span max | 65 ms |

**Bias clustering:** Tags 1–3 cluster at ~-8 ms, Tags 4–10 at
~-6 ms. Likely the per-node-id `retransmit_delay` asymmetry
(600+50·node_id µs → low-id Tags ACK faster). Noted, not yet
equalised.

### 2.2 Throughput (Phase C baseline, all 10 Tags on v17)

| Metric | Value |
|---|---:|
| Per-Tag TX rate mean | 43.5 Hz (target 50 Hz) |
| Combined aggregate | 429 Hz of 500 nominal (**86 %**) |

Collision-limited on shared pipe 0. Phase C bought +16 % per-Tag
vs. the pre-hardening baseline.

### 2.3 Hub-reset recovery

Mid-stream Hub reset costs each Tag **~2 s of frames**, then
immediate resume at pre-fault rate. Repeatable 4× in a row with no
cumulative degradation. CDC disconnect is caught by
`capture_mocap_bridge_robust.py`; wall-clock elapsed preserved
across the gap.

---

## 3. Architecture

### 3.1 ESB topology

```
     Tag 1 ──┐
     Tag 2 ──┤
     Tag 3 ──┤
        …    │    pipe 0 (shared address, 2 Mbps DPL)
     Tag 9 ──┤
     Tag 10──┘
             └──> Hub (PRX) ── USB CDC ──> PC
                      │
                      └── ACK payload carries sync anchor
```

Each Tag TXes on pipe 0 with the same base address. Hub PRX
receives and responds with an ACK that can optionally carry a
payload (the sync anchor). Tag reads the anchor's fields to compute
its clock offset to the Hub.

### 3.2 Wire format — HelixSyncAnchor timeline

| Version | Bytes | Added | Why |
|---|---:|---|---|
| v1 | 8 | base fields (type, central_id, seq, session, rx_ts) | initial |
| v2 | 9 | `flags` (OTA_REQ bit) | OTA trigger path |
| v3 | 10 | `ota_target_node_id` | per-Tag OTA filter under flood |
| v4 | 11 | `rx_node_id` | Stage 2 cross-contamination correctness filter |
| v5 | 16 | `rx_frame_sequence` (u8) + `anchor_tx_us` (u32) | Stage 3 midpoint RTT estimator |

Backward compat: Tag's `node_handle_anchor()` has size gates for
all five versions. Tag on a v3 Hub ignores the v4/v5 fields and
behaves like a v3 Tag. Field layouts and byte tables live in
[`RF_PROTOCOL.md`](RF_PROTOCOL.md).

### 3.3 Sync estimator (Stage 3 midpoint math)

On each accepted anchor (Stage 2 filter: `rx_node_id == g_node_id`):

```
T_tx_us  = tx_ring[anchor.rx_frame_sequence].local_us  // Tag's TX time
T_rx_us  = now_us()                                    // Tag's ACK RX time
H_rx_us  = anchor.central_timestamp_us                 // Hub's frame RX time
H_tx_us  = anchor.anchor_tx_us                         // Hub's ACK TX time

tag_mid  = (T_tx_us + T_rx_us) / 2
hub_mid  = (H_rx_us + H_tx_us) / 2
offset   = tag_mid - hub_mid
```

Queue latency on either side cancels via the midpoint; the residual
error is pure RTT variance (~sub-ms) plus clock drift over the
round trip (1 ppm × 30 ms = 30 ns — negligible).

### 3.4 The ESB ACK-payload FIFO (the deep problem)

Central architectural pain point, *the* reason Stages 2 and 3 exist:

**The PRX ACK-payload FIFO on nRF ESB is per-pipe, not per-Tag.**
When Hub calls `esb_write_payload(anchor_for_Tag_A)` but misses the
~150 µs TIFS window, the payload stays in pipe 0's FIFO. The NEXT
frame from ANY Tag on pipe 0 gets that queued ACK. So the anchor
built from Tag A's frame gets delivered to whoever TXes next — only
by chance Tag A.

**Stage 2 measurement:** out of 34,659 anchors delivered to Tag 1
over 10 minutes, only **494 (1.4 %) were built from Tag 1's own
frame**. The other 98.6 % were cross-contaminated — built from
other Tags' frames.

That's why Stage 3's midpoint math also needed `rx_frame_sequence`
(Hub echoes Tag's seq so Tag can pair against its own ring) — so
even cross-contaminated anchors could be rejected at the source.

---

## 4. User-answered design questions (binding)

These answers from the user (2026-04-22) constrain future design.

| # | Question | User's answer |
|---|---|---|
| 1 | v1 motion-profile — slow mocap only, or fast too? | **Both.** Both use cases on road-map; sync fix has to cover fast. |
| 2 | PC-side fusion depending on wire format? | **Not started yet.** Wire changes have zero downstream cost. |
| 3 | Multi-Hub needed soon? | **No**, not expected in the next 6 months. |
| 4 | What does NCS ESB driver give us for TX-done timestamps? | **Nothing useful.** `esb_evt` on TX_SUCCESS carries only `{evt_id, tx_attempts}` — no hardware TX time. `now_us()` in the handler gives sub-ms jitter at best. This is why Stage 3's `anchor_tx_us` is a queue-time estimate, not a true TX-time one. |
| 5 | Tag-5 physical identification | **Tag 5 = HTag-0126**, node_id=5. Physical inspection left to user. Not currently blocking (Tag 5 is functional on v17). |
| 6 | Write all findings somewhere an agent can learn from | This doc (+ `docs/reviews/`) satisfies that directive. |

### Assumptions currently baked in

- **10 Tags, 1 Hub**, all on ESB pipe 0 (one shared ACK FIFO —
  the whole reason Stages 2 and 3 exist).
- **No per-Tag pipe addresses** — blocked by 8-pipe hardware limit
  vs. 10-Tag fleet.
- **50 Hz per-Tag TX target**, 500 Hz aggregate; achieving 86 %.
- **Retransmit delay per-node-id** (`600 + 50·node_id µs`, so
  650–1100 µs across the fleet). Creates the ~2 ms bias clustering
  seen in §2.1.
- **Clock drift ~1 ppm** (nRF52 HF internal oscillator). Between
  sync updates at 1 Hz, drift is < 1 µs — irrelevant.
- **MCUboot is OVERWRITE_ONLY.** Swap mode parked (separate work).
- **Tag node_id is flash-provisioned** at `0xFE000` (first word of
  the `settings_storage` partition). OTA never touches it.
- **ESB event handler runs in single-core ISR context** on nRF52.
  No locking needed between producer/consumer of the Hub's
  anchor-queue ring or Tag's tx_ring.

---

## 5. What's proven

### 5.1 Hub-relay OTA — solved

- 100 % on 10-Tag fleet across two consecutive full rounds.
- ~240 s per Tag sequential; ~40 min full fleet.
- Single signed firmware binary; `node_id` flash-provisioned.
- PC → USB → Hub → BLE → Tag path, validated at tag
  `ota-reliable-v1` (commit `b112c2f`). Failure modes fully
  characterised and fixed (4 s LFRC pre-settle, per-chunk DATA_RSP
  back-pressure, ATT MTU auto-exchange).
- Detailed fix list in [`NRF_HUB_RELAY_OTA.md`](NRF_HUB_RELAY_OTA.md).

**Don't touch the OTA path** when working on RF.

### 5.2 ESB steady-state throughput (Phase C)

Commit `eaeb79a` landed per-Tag staggered `retransmit_delay` +
boot-time TX phase offset (`200·node_id µs`) + `retransmit_count`
6→10. Result: +16 % per-Tag, +9 % combined.

### 5.3 Hub-reset recovery (Phases B + D)

Mid-stream Hub reset → uniform ~2 s dropout → immediate resume.
4× reset stress in 10 min shows no cumulative degradation.
`capture_mocap_bridge_robust.py` survives CDC disconnect.

### 5.4 Flash-provisioned Tag node_id

Each Tag's identity is one byte at `0xFE000`. OTA never touches
this page; node_id survives firmware rolls. Sentinel `0xFFFFFFFF`
→ falls back to Kconfig default.

### 5.5 Stage 2 correctness filter (v4 anchor)

`rx_node_id` filter. Tag rejects the 98.6 % of anchors that were
built from another Tag's frame. No more cross-contamination of the
sync estimator.

### 5.6 Stage 3 midpoint RTT (v5 anchor)

30× reduction in the 10–30 ms offset-step bucket (12.0 % → 0.4 %).
Residual jitter is real RTT variance. `seq_lookup_miss = 0` across
~500 lookups in the Tag-1 sample — ring depth 16 is adequate.

### 5.7 Stage 3.6 tx_ring ordering race fix

Moving tx_ring publish BEFORE `esb_write_payload`, with `__DMB()`
barrier on the `valid=1` store, recovered Tags 3 and 9 from
multi-second sync degradation on v16. TX-failure unwind path
prevents phantom ring entries.

---

## 6. Measurement methodology

### 6.1 Key metrics (what they mean)

- **Rate (Hz per Tag)** — frames Hub received / capture seconds.
  Nominal 50 Hz. Any shortfall is TX loss (collision, retransmit
  exhaust, or Tag in OTA).
- **Per-Tag sync error** — `frame.sync_us - frame.rx_us`. Tag's
  synced clock minus Hub's RX wall clock. Bias tells you the
  systematic offset; |err| p99 tells you the jitter.
- **Cross-Tag span** — per 50 ms wall-clock bin (≥ 5 Tags in the
  bin), `max(sync_us) - min(sync_us)`. Stale Tags don't contribute
  to bins where they didn't TX (old single-field metric pathologically
  blew up when a Tag stopped; that field is deprecated).

### 6.2 Tools

| Tool | Purpose |
|---|---|
| `tools/analysis/capture_mocap_bridge_robust.py` | Live capture from Hub CDC; resilient to CDC disconnect (Hub reset). Emits per-Tag CSV. |
| `tools/analysis/sync_error_analysis.py` | Per-Tag + bin-based cross-Tag span. `--exclude <node>` filter. |
| `tools/analysis/summary_histogram.py` | Parses Hub's live SUMMARY lines into ack-latency buckets; `--live` mode for direct streaming. |
| `tools/analysis/analyse_mocap_gaps.py` | Per-Tag gap finder (useful after fault injection). |
| `tools/nrf/build_mocap_bridge.sh` | Canonical build (node or central; ProPico or dongle). |
| `tools/nrf/fleet_ota.sh` | Round-based Hub-relay OTA harness (bumps version per round, verifies Tag 1 slot0 via SWD). |
| `tools/nrf/flash_tag.sh` | SWD-flash + provision Tag node_id. |

### 6.3 Measurement recipes

**Cross-Tag span (5-min capture):**
```bash
python3 tools/analysis/capture_mocap_bridge_robust.py /dev/ttyACM1 \
    --csv frames.csv --summary summary.txt \
    --sample-seconds 300 --expected-nodes 10 --settle-seconds 30
python3 tools/analysis/sync_error_analysis.py frames.csv
```

**Hub ack-TX latency histogram (live):**
```bash
python3 tools/analysis/summary_histogram.py --live /dev/ttyACM1 --seconds 600
```

**Tag-1 in-RAM state via SWD** (symbol addresses for v17 build —
regenerate via `arm-none-eabi-nm -n` after any rebuild):
```
midpoint_offset_us      0x20002BD8   (int32)
midpoint_offset_valid   0x20002BDC   (u32, 0 or 1)
midpoint_step_bucket    0x20002BE0   (4 × u32)
seq_lookup_miss         0x20002BF0   (u32)
anchors_wrong_rx        0x20002C78   (u32)
offset_step_bucket      0x20002C7C   (4 × u32)
anchor_age_bucket       0x20002C8C   (4 × u32)
anchors_received        0x20002CCC   (u32)
```
Read via:
```bash
nrfutil device read --serial-number 123456 --address 0x20002BD8 --bytes 4 --direct
```

---

## 7. Design history (condensed)

### 7.1 Phase A — Baseline measurement harness (2026-04-20)

Built capture + analysis tooling. Established pre-hardening
baseline: 41.5 Hz/Tag mean, 399 Hz combined, 50 ms cross-Tag span
p99 on the misleading old metric (later fixed).

### 7.2 Phase B — Hub-reset characterisation

Mid-stream Hub reset gives uniform ~2 s dropout, no cascading
failure. Built the robust capture tool that survives CDC
re-enumeration.

### 7.3 Phase C — Collision hardening (commit `eaeb79a`)

Per-Tag `retransmit_delay` (650–1100 µs spread), `retransmit_count`
6→10, boot-time TX phase offset (`200·node_id µs`). Result: +16 %
per-Tag, +9 % combined.

### 7.4 Phase D — Multi-reset stress

Four injected Hub resets in 10 minutes → each Tag sees ~2.1 s gap
at each reset event, no cumulative degradation.

### 7.5 Stage 1 / Stage 1' — RF-sync instrumentation (2026-04-22)

Added Hub `ack_lat` bucketed histogram (how long an anchor spent
queued before TX) and Tag `anchor_age`/`offset_step` buckets. First
impl had a measurement bias (single-slot queue timestamp); Stage
1' replaced it with a 16-deep ring to measure the older queued
anchors' latencies properly.

Finding: >70 % of ACK TXs land 10–30 ms late — confirms
slow-path-dominance hypothesis. Drove the Stage 3 design.

### 7.6 Stage 2 — v4 anchor with `rx_node_id` (v14 firmware)

Hub stamps `rx_node_id = frame->node_id` when building each anchor.
Tag filters: drop the sync update if `rx_node_id != g_node_id`.

Measurement surprise: 98.6 % of anchors rejected, not the 70–90 %
expected. Revealed that `rx_node_id` labels "which Tag caused the
anchor to be built" (at queue time), NOT "which Tag will receive
it" (the FIFO returns oldest queued to next TX). These coincide
only when FIFO depth is 1 at next TX — rare with 10 Tags sharing
pipe 0.

Net: correctness is preserved (matched anchors genuinely trace to
our own RX time), effective anchor-accept rate drops from ~60 Hz
to 0.9 Hz per Tag. Still correct — at 1 ppm drift, 1 Hz is plenty
— but ACK-TX latency bias still pollutes every accepted update.

### 7.7 Stage 3 — v5 anchor with midpoint RTT (v15 firmware)

Hub adds `rx_frame_sequence` (echoes Tag's seq) + `anchor_tx_us`
(Hub's `now_us()` just before `esb_write_payload`). Tag maintains
a 16-slot ring `{seq, local_us}`. Midpoint math cancels queue
latency on both sides.

Measurement (Tag 1): `mid_step` bucket vs old `offset_step`:
- <2ms:   58.4 % vs 46.6 % (+25 %)
- 2–10ms: 39.9 % vs 39.8 % (flat)
- **10–30ms: 0.4 % vs 12.0 % (30× reduction)**
- ≥30ms:  1.4 % vs 1.5 % (flat)

`seq_lookup_miss = 0`. Ring depth 16 adequate.

### 7.8 Stage 3.5 — midpoint in frame sync_us (v16 firmware)

Previously Tag emitted `synced_us = local_us - estimated_offset_us`
(v3 math) in its mocap frame. Switched to use `midpoint_offset_us`
when `midpoint_offset_valid == 1`, fall back to estimated_offset
for the first-second bootstrap.

Measurement (7-Tag healthy cohort): fleet mean bias -7816 µs with
**±0.4 ms Tag-to-Tag spread**. Exceptional systematic consistency —
confirms midpoint math is working. Cross-Tag span p99 was still
~46 ms because per-Tag RTT jitter dominates (not the bias).

### 7.9 Stage 3.6 — tx_ring push ordering (v17 firmware)

Tag's `maybe_send_frame()` previously pushed the new {seq,tx_us}
to the ring AFTER `esb_write_payload` returned. The anchor RX ISR
could fire before the push completed, causing `seq_lookup_miss`
and a never-locked midpoint on some Tags.

Fixed by pushing BEFORE `esb_write_payload`, with `__DMB()` between
the data stores and the `valid=1` store (publication ordering).
TX-failure unwind path to avoid phantom ring entries.

**Result: Tags 3 and 9 recovered** from their multi-second |err|
p99 on v16 to normal 13.5–18 ms range.

### 7.10 v17 20-min long-run validation

Full 10-Tag fleet, 30-s settle: per-Tag |err| p99 13.5–17.5 ms
across every Tag. Tag 10 (previously thought to be hardware) also
came in at normal 17 ms p99 — the 2.14 Hz TX rate seen earlier was
a post-OTA transient, not a persistent hardware issue.

**Clean baseline.** Cross-Tag span p99 53.5 ms — the architectural
limit of shared pipe 0.

### 7.11 Stage 3.7 — retry-count instrumentation (v18 firmware)

Reviewer hypothesis (round 5, Codex + Copilot): the residual 1.4 %
`≥30 ms` bucket in `mid_step` might be driven by Tag-side retry
ambiguity (TX ring records original TX time; successful retry
happens N × retransmit_delay later). Rather than land Stage 3.5
speculatively, instrument first.

v18 added a `retry_count` byte to `tx_stamp`, back-filled at
TX_SUCCESS from `esb_evt.tx_attempts`. A 2D histogram
`mid_step_by_retry[4][4]` on Tag records joint distribution.

**Measurement (Tag 1 SWD readout after 20-min v18 capture):**

|  | <2 ms | 2–10 ms | 10–30 ms | ≥30 ms | row tot |
|---|---:|---:|---:|---:|---:|
| Retry=1 | 287 | 98 | 1 | 0 | 386 |
| Retry=2–3 | 95 | 56 | 3 | 1 | 155 |
| Retry=4–6 | 103 | 109 | 0 | 3 | 216 |
| Retry=7+ | 241 | 232 | 1 | 4 | 478 |

**Conclusion:** retries are NOT the driver. Even retry-1 (first
attempt) produces 25 % 2–10 ms jitter. The ≥30 ms column has only
8 events, spread across retry buckets. **Skip Stage 3.5**; go to
Stage 4. Full doc: `docs/RF_V18_FINDINGS.md`.

### 7.12 Stage 4 — Tag-side TDMA Path X1 (v19 → v20)

Design goal: give each Tag its own 2 ms TDMA slot within a 20 ms
cycle (10 Tags × 2 ms). Path X1 = no separate broadcast beacon
(anchors already carry `anchor_tx_us`); Tag uses its own Stage 3
midpoint as the time reference, schedules TX via `k_usleep`.
Design in `docs/RF_STAGE4_EXPLORATORY.md`.

Three bumps happened:
- **v19** had a syntax error that `fleet_ota.sh` didn't detect
  (stale `zephyr.signed.bin` deployed instead). Caught via SWD
  readout — no `stage4_*` symbols in the flashed ELF. Hardened
  `fleet_ota.sh` to nuke artifacts before rebuild + check
  build exit code.
- **v19 (real)** had a "skip-forever" bug: `maybe_send_frame()`
  skipped the current iteration when `delay_us > SLOT_US`. But
  main loop's 20 ms k_sleep equals the cycle period, so the next
  iteration lands at the same cycle_phase and skips again. TX
  rate collapsed to < 1 Hz.
- **v20** fixed the skip-forever: always `k_usleep(delay_us)` to
  the next slot (0–20 ms). TX rate recovered to ~30 Hz (down
  from 43 Hz free-run — the `k_usleep` cost).

v20 measurement surprise: despite TDMA active (47 promotions on
Tag 1), cross-Tag span p99 stayed at 49.5 ms (same as v17). Fleet
mean per-Tag bias tightened from ±1.8 ms to ±0.2 ms (real Stage 4
win) but span didn't improve. Root cause discovered next →
Stage 3.7-FIFO1 (§7.13). Full doc: `docs/RF_V20_FINDINGS.md`.

### 7.13 The FIFO=1 discovery — architectural root cause

Traced the Stage 4 v20 no-improvement surprise to a **+10 ms
bias baked into the Stage 3 midpoint estimator** under
`CONFIG_ESB_TX_FIFO_SIZE=8` (the NCS default).

**Mechanism:** midpoint math assumes symmetric RTT. With a deep
FIFO, the Hub→Tag leg includes 0–30 ms of queue time (Stage 1'
showed >70 % of ACK TXs land 10–30 ms late). `anchor_tx_us` was
stamped at `esb_write_payload` call time (queue enter), not at
radio TX time. So `hub_mid = (central_ts + anchor_tx_us)/2` sits
near Hub RX (early), while `tag_mid = (T_tx + T_rx)/2` sits
past physical mid-RTT (late, because T_rx is FIFO-delayed). The
offset baked ~10 ms of one-sided queue latency.

**Fix: one line.** Added `CONFIG_ESB_TX_FIFO_SIZE=1` to Hub's
`central.conf`. Hub can only queue ONE anchor at a time; if TIFS
is missed, the anchor simply doesn't queue (Stage 2 `rx_node_id`
filter absorbs the resulting empty ACKs for other Tags). With
depth 1, round-trip is symmetric, midpoint math is accurate.

**Result (Tag 1, same v20 fleet, just Hub rebuild):**

| Metric | Before FIFO=1 | After FIFO=1 | Δ |
|---|---:|---:|---|
| Mean bias | -10128 µs | -308 µs | −97 % |
| `\|err\|` p50 | 10 ms | 500 µs | −95 % |
| `\|err\|` p99 | 16.5 ms | 4.0 ms | −76 % |
| `\|err\|` max | 24.5 ms | 12.5 ms | −49 % |

**Fleet mean bias centered near zero** (spread ±0.6 ms). Full
doc: `docs/RF_FIFO1_DISCOVERY.md`.

**Cost:** aggregate throughput 428 → 306 Hz (−28 %). Fine for
sync (at 1 ppm drift, 1 Hz is enough) but notable for frame rate.

### 7.14 v21 A/B — Stage 4 TDMA disabled (FIFO=1 alone)

A/B test: does Stage 4 TDMA add anything on top of FIFO=1?
Disabled Stage 4 (`CONFIG_HELIX_STAGE4_TDMA_ENABLE=n`), kept Hub
FIFO=1. Fleet-OTA'd 8/10 Tags to v21 (Tags 8 and 10 stuck on
v19 — ESB trigger can't reach them at <1 Hz TX rate).

| Metric | v20 (Stage 4 ON) | v21 (Stage 4 OFF) |
|---|---:|---:|
| TX rate / Tag | 30 Hz | **49 Hz** |
| Mean bias spread | ±0.6 ms | ±0.5 ms |
| `\|err\|` p99 | 4–9.5 ms | 6.5–9 ms |
| Cross-Tag span p99 | 49.5 ms | **42.5 ms** |

**v21 wins** — recovered the 28 % throughput loss, similar sync
quality, slightly tighter cross-Tag span. Stage 4 TDMA code
preserved in the tree but `default n` in Kconfig.

### 7.15 Overnight soak validation (v20 fleet + Hub FIFO=1)

4-hour soak, 4.2 M rows. Per-Tag `|err|` p99 stable at 9–15 ms
across the soak (no thermal drift, no stuck states). Sparse
outlier events (Tag 6 one 24.6-s event, Tag 1 one 84-s event)
do NOT accumulate. Cross-Tag span p99 49.5 ms stable. Full doc:
`docs/RF_OVERNIGHT_2026-04-23_SUMMARY.md`.

### 7.16 Cross-Tag span fat-tail investigation

Per-Tag `|err|` p99 ~10 ms but fleet cross-Tag span p99 ~42 ms —
a 5× gap. Tool `tools/analysis/find_span_outliers.py` finds
4 184 bins with span > 30 ms (4.9 %, matches p99). Outliers
distributed uniformly across 8 healthy Tags (~500 each).

**Diagnosis:** Stage 3 midpoint occasionally takes a 10–30 ms
step (1.8 % of accepted anchors — see `mid_step_by_retry` row
totals in §7.11). Over 4 h × 0.9 Hz = ~233 large jumps/Tag.
Likely cause: residual cross-contamination bleed-through where an
anchor built from Tag A's frame reaches Tag B AND happens to
have `rx_node_id` byte matching B by chance (1/10 probability).

**Proposed fix (not yet landed):** glitch-reject single-sample
midpoint jumps > 10 ms. Full doc:
`docs/RF_CROSS_TAG_SPAN_INVESTIGATION.md`.

---

## 8. Open decisions (for reviewer proposals)

These are the decisions we need Copilot / Codex / user input on.
Please respond per decision in your review.

### D1. Is sub-ms cross-Tag span (< 1 ms p99) a **hard** v1 requirement?

- If yes → Stage 4 (TDMA) or per-Tag pipes are the path. Both are
  multi-day and architecturally disruptive.
- If no (≤ 10 ms p99 acceptable) → we're essentially done. Stage 3.6
  at 53 ms p99 span / ~15 ms |err| p99 per-Tag is usable for many
  mocap scenarios, especially after PC-side fusion subtracts the
  ~-7 ms systematic bias.

The user-confirmed requirement (rf-sync-requirements.md §TBD) says
< 1 ms. But the motion-capture fusion code hasn't been built yet —
so the real-world tolerance is not yet measured. **We need a call
on whether to invest days in TDMA now or ship Stage 3.6 and tighten
later if downstream fusion says we need to.**

### D2. If D1 is "hard yes": TDMA slots (§9) or per-Tag pipes?

**TDMA** (§9): 20 ms major cycle, 10× 2 ms slots, Hub beacon for
bootstrap sync, Tag uses TIMER for slot alignment. Eliminates FIFO
queuing entirely. 2–3 days implementation + 1 day tuning.

**Per-Tag pipes** (task #35): give each Tag a dedicated ESB pipe
address, so ACK payloads are directed. BLOCKED by 8-pipe hardware
limit vs. 10-Tag fleet. Would require dropping to ≤ 8 Tags/Hub, or
a two-pipe grouping where pairs of Tags share a pipe (but then the
FIFO problem returns within each pair).

**Our leaning:** TDMA is the only clean architectural fix for 10+
Tags. But we want reviewer opinions — is there a third option we're
missing? Multi-frequency? Compressed sensing with irregular slots?

### D3. Retransmit_delay asymmetry — equalise?

Current `config.retransmit_delay = 600 + 50·node_id µs` gives a
bias clustering: Tags 1–3 at ~-8 ms mean, Tags 4–10 at ~-6 ms.
Equalising (all Tags same delay) would shrink the fleet bias spread
from 3.6 ms to ~1 ms — tightens cross-Tag span by ~3 ms.

But the per-node-id spread was Phase C's collision-hardening
feature (+16 % throughput). Removing it might re-introduce
collisions.

**Experiment proposal:** A/B test on the fleet — 5 Tags at fixed
delay (say 650 µs), 5 Tags at staggered. Compare sync bias spread
AND throughput. Low effort (~50 min measurement cycle), could give
a tighter fleet without TDMA.

### D4. What's the test bar for "RF is done"?

Currently untested:
- **Multi-hour soak** (20 min is our longest run)
- **Thermal drift** (die heats up over hours; LF clock aging)
- **2.4 GHz coexistence** (Wi-Fi router, BT headphones, LTE-U)
- **Battery sag / brownout** (Tags on USB today; LiPo 4.2→3.2 V)
- **Charge-while-streaming** (TP4054 at 2.4 GHz cross-talk?)
- **Body shadowing** (Tags on desk ≠ Tags on limbs)
- **Tag mid-stream reset recovery** (only Hub reset tested)

These could each surface a regression our current data doesn't
see. Which are blockers for declaring "RF done" vs. deferrable
to a later "RF hardening pass"?

### D5. What to do with `/tmp/helix_tag_log/` scratch tooling?

Some harness scripts still live in `/tmp/` (cleared on reboot).
`tools/nrf/` has canonical copies of most; one or two are still
scratch-only. Propose: sweep `/tmp/` and either upstream or delete.

### D6. Stage 4 pipe budget

Stage 4 design uses 1 additional ESB pipe for the Hub→Tag beacon.
That's 2 pipes total (pipe 0 for Tag→Hub data, pipe 1 for beacon).
Of the 8 total pipes we'd have 6 left for future uses (per-Hub
isolation, etc.). Acceptable trade-off, or should the beacon share
pipe 0 somehow?

---

## 9. Stage 4 — DELETED in v22

**Status: DELETED (commit `0ae662f`).** Both Codex + Copilot
"team of experts" brainstorms returned **unanimous DELETE**
after v21 A/B (§7.14) showed Stage 4 adds zero sync value at
28 % throughput cost once `FIFO=1` (§7.13) was in place.

Where the Stage 4 artifacts live now:
- **Design rationale:** `docs/archive/rf/RF_STAGE4_EXPLORATORY.md`
- **Implementation reference:** git tag
  `stage4-tdma-path-x1-reference` (commit `c442eb7`, the last
  state with Stage 4 code active in the tree).
- **Findings:** `docs/RF_V19_FINDINGS.md`,
  `docs/RF_V20_FINDINGS.md`, `docs/RF_FIFO1_DISCOVERY.md`.
- **Brainstorm artefacts:**
  `docs/reviews/{codex,copilot}_2026-04-23_stage4_fate.md`.

**If future scaling forces us back to TDMA** (>10 Tags on one
Hub, or PC fusion demands < 5 ms cross-Tag span that FIFO=1
can't deliver), don't resurrect Path X1. Both reviewers agreed
the next TDMA design should be:
- Beacon-based (Hub broadcasts time reference on a separate ESB
  pipe), not estimator-self-referential.
- Hardware TX timestamping via PPI + nRF52 TIMER compare, not
  `now_us()` in an ISR.
- Explicit acquisition / loss semantics and measured air-slot
  guarantees.

### 9.1 Why TDMA

Stage 3.6 delivered tight per-Tag |err| but left cross-Tag span at
53 ms p99 because all 10 Tags collide on shared pipe 0. TDMA gives
each Tag its own air slot — at most one Tag transmits at a time, so
the ACK-payload FIFO never builds up, cross-contamination drops to
near 0 %, and `anchor_tx_us` becomes accurate to TIFS hardware time.

### 9.2 Slot architecture

```
20 ms major cycle, 10 × 2 ms slots (one per Tag):
  |Tag1|Tag2|Tag3|Tag4|Tag5|Tag6|Tag7|Tag8|Tag9|Tag10|
    0    1    2    3    4    5    6    7    8    9
```

Each 2 ms slot:
- 0.0–0.3 ms: guard + wakeup
- 0.3–0.4 ms: ESB frame (32 B @ 2 Mbps)
- 0.4–0.6 ms: TIFS + Hub ACK TX
- 0.6–0.8 ms: retry window
- 0.8–2.0 ms: quiet

Slot assignment: `node_id` → slot index (1-based). Number of slots
configurable via `CONFIG_HELIX_MOCAP_TDMA_SLOTS`.

### 9.3 Bootstrap (Stage 3 → Stage 4 transition)

TDMA requires sub-ms knowledge of Hub's clock. Stage 3 delivers
17 ms |err| p99 — not tight enough. Solution:

- **Hub beacon** every 20 ms cycle in a reserved pre-slot. Beacon
  carries `hub_us` at TX time. One additional ESB pipe (pipe 1),
  broadcast address.
- **Tag EMA filter** on beacon RX to converge to sub-ms over
  10–20 cycles (~200–400 ms).
- Once `|Δoffset| < 500 µs` for 5 consecutive cycles, Tag
  transitions to TDMA slot emission.
- During bootstrap Tag stays in Stage 3 free-run ESB mode.

### 9.4 Expected performance

- Slot collisions: 0 (by design)
- Cross-Tag span p99: < 1 ms
- Per-Tag |err| p99: < 1 ms
- Meets v1 requirement.

### 9.5 Failure modes

- **Beacon miss:** N missed → EMA stale → fall back to Stage 3 until
  lock re-acquires.
- **Slot collision from unsynced Tag:** Harmless (ESB retries); Hub
  assigns slot only after confirming lock quality via handshake.
- **Hub reboot:** Tags re-bootstrap in ~400 ms.

### 9.6 Implementation staging

1. Beacon TX on Hub (Zephyr work-queue, TIMER-driven).
2. Beacon RX + EMA filter on Tag.
3. `stage4_offset_us` plumbed into frame sync_us (parallel with
   Stage 3 midpoint).
4. A/B split: 5 Tags on Stage 3, 5 on Stage 4. Compare.
5. TIMER-driven slot scheduling on Tag.
6. 10-Tag TDMA validation.
7. Tune slot width below 2 ms if safe.

Each step individually testable. Stage 3 remains fallback throughout.

### 9.7 Cost

2–3 days firmware + 1 day hardware tuning. Not a quick win —
**confirm D1 first.**

---

## 10. Known gaps / untested / deferred

| Area | Status | Note |
|---|---|---|
| Multi-hour soak | Untested | Longest run = 20 min |
| Thermal drift | Untested | Die heats up over hours |
| 2.4 GHz coexistence | Untested | Wi-Fi / BT interference |
| Battery sag / brownout | Untested | Tags on USB today |
| Charge-while-streaming | Untested | TP4054 + RF cross-talk |
| Body shadowing | Untested | Benchtop ≠ worn |
| Tag mid-stream reset | Untested | Only Hub reset tested |
| Tag 1 startup artefact | Cosmetic | Few frames before midpoint lock pollute mean; p99 clean |
| retransmit_delay asymmetry | Noted | ~2 ms bias clustering Tags 1–3 vs 4–10; see D3 |
| Stage 3 slow-path "rare anchor" (1.4 % ≥30 ms) | Noted | Likely Tag-retry ambiguity; could be fixed with per-retry ring update if needed |
| `SYNC_SPAN_US` field in old capture tool | Deprecated | Use `sync_error_analysis.py` instead |

---

## 11. Outstanding tasks (from task tracker)

| # | Title | State |
|---|---|---|
| 41 | Stage 4 TDMA slots for sub-ms cross-Tag span | Pending — gated on D1 |
| 35 | Phase E: per-Hub ESB pipe derivation | Deferred — multi-Hub not needed for 6 months |
| 21 | Parallel OTA | Parked by user |
| — | Tag 10 reliability probe | Recovered on own; optional follow-up |
| — | Retransmit_delay A/B equalise test | Proposal in D3 |
| — | Multi-hour soak / coexistence / battery | Per D4 |

---

## 12. Reviewer rounds index

| Round | Prompt | Responses |
|---|---|---|
| 1 | `RF_NEXT_STEPS_DESIGN_BRIEF.md` (archived) — original v4-anchor proposal | `docs/reviews/copilot_2026-04-21.md`, `docs/reviews/codex_2026-04-21.md` |
| 2 | Follow-up on round-1 + Stage 1 implementation | `docs/reviews/copilot_2026-04-22_followup.md`, `docs/reviews/codex_2026-04-22_followup.md` |
| 3 | Stage 1 measurement-bias interpretation | `docs/reviews/copilot_2026-04-22_round3.md`, `docs/reviews/codex_2026-04-22_round3.md` |
| 4 | Stage 2 rejection-ratio surprise (98.6 %) | `docs/reviews/round4_stage2_measurement_prompt_2026-04-22.md` — **prompt only, unreviewed** |
| 5 | Stage 3 midpoint results + Stage 4 ask | `docs/reviews/round5_stage3_results_prompt_2026-04-22.md` — **prompt only, unreviewed** |

Rounds 4 and 5 prompts are ready for the user to forward to
Copilot/Codex. This document (RF.md) supersedes earlier sprint
docs and can itself serve as the round-6 prompt once the user
wants a fresh reviewer pass.

---

## 13. Hardware / test-rig

- **Hub:** nRF52840 dongle on J-Link SN `69656876`. Enumerates as
  `/dev/ttyACM1` (USB CDC, VID/PID Nordic). Board target
  `nrf52840dongle/nrf52840/bare` (the `bare` variant — do NOT build
  plain `nrf52840dongle/nrf52840`, the CDC never comes up).
- **Tag 1:** nRF52840 ProPico on J-Link SN `123456` (J-Link Plus).
  The only Tag with SWD access. Used for in-RAM histogram reads.
- **Tags 2–10:** USB-powered only (no SWD). All identified by FICR
  suffix (HTag-XXXX); mapping in `/tmp/helix_tag_log/programmed.txt`.
- **Datasheets in tree:** `datasheets/TP4054.pdf` (charge IC),
  `datasheets/propico_schematics.jpg` — added during Tag 1 blue-LED
  debug; useful for any future Tag-physical-inspection work.

---

## 14. Commit chain (RF-relevant)

Most recent first:

```
74ad5e6  m8-rf: persist user Q&A + round 4/5 reviewer prompts
d251c3f  m8-rf: v17 20-min validation — all 10 Tags healthy
c9857db  m8-rf: Stage 3.6 recovered Tags 3/9 via ring-push race
804e86a  m8-rf: Stage 3.6 — tx_ring push before esb_write_payload
234e040  m8-rf: Stage 3.5 findings — fleet bias -7.8ms ±0.4ms
99adc41  m8-rf: Stage 3.5 — midpoint_offset_us in frame sync
acf315b  m8-rf: Stage 3 findings — midpoint 30x reduction
94e1d05  m8-rf: Stage 3 — v5 anchor with midpoint RTT estimator
de67822  m8-rf: Stage 2 findings — v4 rejection ratio = 98.6%
ae3cc97  m8-rf: Stage 2 — v4 anchor with rx_node_id
5d79e9a  m8-rf: Stage 1' findings — Hub unbiased ack_lat
0d9708a  m8-rf: Stage 1' — Hub ack-latency ring buffer
5f02792  m8-rf: Stage 1 — ACK-TX latency + anchor_age instrumentation
7530732  m8-rf: upstream /tmp/ harness + sync-metric fix
eaeb79a  m8-rf: Phase C collision hardening
b112c2f  m8: Hub-relay OTA 5/5 reliable (tag ota-reliable-v1)
```

Tag `ota-reliable-v1` at `b112c2f` is the "known good OTA" anchor.
Anything from `eaeb79a` onwards adds RF-side improvements.

---

## 15. Getting oriented as a new agent / reviewer

1. **Read this doc top to bottom.** You now have full context.
2. **For wire-format details**, see [`RF_PROTOCOL.md`](RF_PROTOCOL.md).
3. **For requirements**, see [`rf-sync-requirements.md`](rf-sync-requirements.md).
4. **For OTA** (it's solved, don't touch), see
   [`NRF_HUB_RELAY_OTA.md`](NRF_HUB_RELAY_OTA.md).
5. **For historical dialog** (round 1–3 decisions), see
   `docs/reviews/`.
6. **For archived earlier drafts** that this doc superseded, see
   `docs/archive/rf/`.

**If you're being asked to propose a path forward**, answer the
questions in §8 (D1–D6). Those are the open decisions.
