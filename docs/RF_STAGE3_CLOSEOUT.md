# RF Sync Stage 3 closeout (2026-04-22)

Single-page status for anyone picking up the RF sprint. For the full
debate and measurements, see
[RF_SYNC_DECISION_LOG.md](RF_SYNC_DECISION_LOG.md).

## What's on the fleet right now

| Part | Version | Location |
|---|---|---|
| Hub firmware | v5 anchor, Stage 3 midpoint | SWD-flashed, `nrf52840dongle/nrf52840/bare` |
| Tag firmware | v16 (midpoint wired into frame sync_us) | OTA fleet 10/10 PASS |
| Commit at close | `234e040` |

All 10 Tags run the same signed binary; `node_id` is flash-provisioned
at `0xFE000`.

## Wire-format timeline

| Version | Size | New field | Purpose |
|---|---:|---|---|
| v1 | 8 B | — | base anchor |
| v2 | 9 B | `flags` | OTA_REQ bit |
| v3 | 10 B | `ota_target_node_id` | per-Tag OTA filter |
| v4 | 11 B | `rx_node_id` | Stage 2 cross-contamination filter |
| v5 | 16 B | `rx_frame_sequence`, `anchor_tx_us` | Stage 3 midpoint RTT |

Tags still accept all older formats for rolling upgrades.

## Measured sync quality (Stage 3.5, v16)

**7 healthy Tags, 5-min capture, 125k frames:**
- Fleet mean per-Tag bias: **−7816 µs** (range −8218 .. −7453, spread ±0.4 ms)
- Per-Tag |err| p99: **13.5 – 19.0 ms**
- Cross-Tag instantaneous span p50/p99: **19.5 / 46.5 ms**

**Degraded Tags (excluded from above):**
- Tag 3: |err| p99 = 1.38 s — midpoint did not lock cleanly at boot.
- Tag 9: |err| p99 = 2.0 s — similar.
- Tag 10: TX rate 2.14 Hz — hardware/link issue, not sync.

## What worked

1. **Stage 2 `rx_node_id`** — correctness filter. 98.6 % of anchors
   delivered to any Tag were built from another Tag's frame; filter
   prevents them from corrupting the sync estimator.
2. **Stage 3 midpoint RTT with `anchor_tx_us` + `rx_frame_sequence`**
   — cancels Hub-side ACK-TX queue-latency bias. 30× reduction in
   the 10-30 ms offset-step bucket (12.0 % → 0.4 %).
3. **Stage 3.5** — switching Tag's `sync_us` frame field to use
   `midpoint_offset_us` gave the PC-visible span numbers above.

## Gap to requirements

`rf-sync-requirements.md` lists **< 1 ms p99 cross-Tag** as the v1
target. We're at **46.5 ms p99** with 7 healthy Tags — ~46× off.

Levers still on the table:
1. **Stage 4 TDMA slots** (biggest lever). Each Tag TXes in its own
   ~2 ms slot within a 20 ms cycle. Eliminates shared-pipe FIFO
   queuing entirely. Needs bootstrap sync from Stage 3. See task #41.
2. **Per-Hub ESB pipe addresses** (task #35). Gives each Tag its own
   ACK FIFO. Blocked today by 8-pipe hardware limit vs. 10-Tag
   fleet requirement; would require dropping to ≤ 8 Tags/Hub.
3. **Stage 3.6 fix: ring-push ordering** (task #40). Closes a
   potential race where an anchor RX ISR could fire before the Tag's
   TX timestamp is pushed into the seq-indexed ring. Minor — not
   known to trigger, but cheap to fix.

## Outstanding issues

| # | Title | Impact |
|---|---|---|
| 39 | Degraded Tags 3, 9, 10 | 3/10 of fleet bad sync |
| 40 | Stage 3.6 ring-push ordering | Potential race, unproven |
| 41 | Stage 4 TDMA design | Architectural next step |
| 35 | Per-Hub pipe derivation | Deferred (multi-Hub not needed yet) |
| 21 | Parallel OTA | Parked by user |

## Quick reference — SWD RAM read for Tag 1 (v16 build)

```
midpoint_offset_us      0x20002BD8 (int32)
midpoint_offset_valid   0x20002BDC (u32, 0 or 1)
midpoint_step_bucket    0x20002BE0 (4 × u32)
seq_lookup_miss         0x20002BF0 (u32)
anchors_wrong_rx        0x20002C78 (u32)
offset_step_bucket      0x20002C7C (4 × u32)
anchor_age_bucket       0x20002C8C (4 × u32)
anchors_received        0x20002CCC (u32 — g_helixMocapStatus + 0x24)
```

Regenerate for any future build via:
```
arm-none-eabi-nm -n .deps/ncs/v3.2.4/build-helix-promicro_nrf52840-nrf52840-mocap-node-node1/nrf52840-mocap-bridge/zephyr/zephyr.elf | grep -E 'bucket|anchors|midpoint|seq_lookup'
```

## Measurement recipes

**10-min in-RAM histogram (Hub side, Tags accessible via SWD):**
```
python3 tools/analysis/summary_histogram.py --live /dev/ttyACM1 --seconds 600 > capture.txt
# Then SWD-read Tag RAM for Tag-side histograms (see addresses above)
```

**5-min cross-Tag span (Hub CDC, full FRAME data):**
```
python3 tools/analysis/capture_mocap_bridge_robust.py /dev/ttyACM1 \
    --csv frames.csv --summary frames_summary.txt \
    --sample-seconds 300 --expected-nodes 10
python3 tools/analysis/sync_error_analysis.py frames.csv --exclude 3 9 10
```
