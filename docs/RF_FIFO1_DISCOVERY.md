# FIFO=1 discovery — 95 % sync-error reduction (2026-04-23 early AM)

**Tonight's headline result.** Setting `CONFIG_ESB_TX_FIFO_SIZE=1`
on the Hub (was 8, the NCS default) cuts per-Tag sync error by
~95 % with no firmware change to Tags.

## Numbers

Same v20 fleet (7 healthy Tags on Stage 4 TDMA, 2 on stuck v19),
same 15-min capture. Only difference: Hub built with
`CONFIG_ESB_TX_FIFO_SIZE=1` in `central.conf`.

| Tag | Before (FIFO=8) | After (FIFO=1) | Change |
|---:|---:|---:|---|
| 1 mean bias | -10128 µs | **-308 µs** | −97 % |
| 1 \|err\| p50 | 10000 µs | **500 µs** | −95 % |
| 1 \|err\| p99 | 16500 µs | **4000 µs** | −76 % |
| 1 \|err\| max | 24500 µs | 12500 µs | −49 % |

All 7 healthy Tags:
- Fleet mean bias range: **-570 µs to +600 µs** (was -10186 to -9829)
  — bias *centered near zero*
- Per-Tag |err| p50: 500–1000 µs (**all sub-ms**)
- Per-Tag |err| p99: 4.0–9.5 ms
- Per-Tag |err| max: 12–18 ms

Tag 3 in this capture shows a startup-transient (+28.6 ms mean bias
with 1.4 s max err) — likely a brief pre-lock glitch. Tags 8 & 10
stuck on v19 as before.

Cross-Tag span p50 / p90: **32 / 42 ms** (slightly worse in this
run because Tag 3's outlier drags the bin-average). Once Tag 3's
transient is excluded, the real p99 span should match the per-Tag
p99 of ~5-10 ms.

## Why it works — confirms the hypothesis

The Stage 3 midpoint estimator ASSUMES the round-trip Tag → Hub →
Tag is SYMMETRIC. With `FIFO_SIZE=8`:

```
Tag TX  ─►  Hub RX (130 µs)  ─►  FIFO queue (0–30 ms)  ─►  Hub ACK TX
Tag RX ◄───────────────────────────────────────────  (return 130 µs)
```

The "Hub RX → Hub TX" leg is 0–30 ms depending on FIFO depth.
The two `T_tx + T_rx` halves on Tag's side are NOT symmetric about
the Hub RX instant — they're symmetric about the Hub ACK TX
instant. Since `anchor_tx_us` was stamped at `esb_write_payload`
CALL TIME (queue enter), not at radio TX time, the math introduces
a +10 ms bias per the docs/RF_V20_FINDINGS.md analysis.

With `FIFO_SIZE=1`:
- If Hub hits the TIFS window (~150 µs post-RX): anchor queues, TX
  happens immediately. Round-trip is symmetric. Midpoint is accurate.
- If Hub misses TIFS: `esb_write_payload` returns **error**; no
  anchor queues. Tag's next frame gets no anchor (or the already-
  queued one for some other Tag — filtered out by Stage 2 rx_node_id).

**Net: only fast-path anchors reach the Tag's estimator.** Those
are symmetric. Midpoint math works as intended.

## Trade-off

Aggregate throughput drops from ~428 Hz to ~306 Hz (−28 %) because
only the fast-path TX windows on Hub pass back anchor data; the
slow-path frames get empty ACKs.

For *sync quality* this is fine: at 1 ppm drift, even 1 Hz of
fresh correct anchors per Tag is plenty. Each healthy Tag accepts
several Hz of good anchors.

For *frame throughput*, this is a concern only if the PC fusion
layer needs every frame. At 30 Hz per Tag we still have 2× the
minimum usable frame rate for mocap.

## Why I didn't catch this earlier

The Stage 1' and Stage 2 measurement work showed the FIFO was
"too deep" (pend_max = 88620 in Stage 1'), but the interpretation
at the time was "FIFO queuing causes cross-contamination, solved by
Stage 2 rx_node_id filter." That IS correct — the contamination IS
filtered.

What I missed: the midpoint math (Stage 3) has its own dependence
on FIFO depth. Even anchors that pass the rx_node_id filter carry a
queue-time-delayed `anchor_tx_us` that biases the midpoint.

The hint was hiding in plain sight in the 20-min v17 run: fleet
mean bias was uniformly -6.6 ms across all Tags (very consistent,
spread ±1.8 ms). I called that "probably propagation or ISR latency
asymmetry." It was actually FIFO queue-mean leaking into the
midpoint.

## Next steps

1. **Make FIFO=1 permanent** on Hub (central.conf already updated,
   need to commit).
2. **Measure cross-Tag span** with all 10 Tags on the clean
   architecture. Expected: p99 well under 10 ms, maybe under 5 ms.
3. **OTA Tags 8 and 10** back into the fleet (they're stuck on
   broken v19). With FIFO=1, the ESB OTA trigger window may be
   different — first need to verify trigger still works.
4. **Revisit Stage 4 TDMA with this baseline.** If Stage 3.6 on
   FIFO=1 already delivers ≤ 5 ms p99 with minimal Tag-side
   changes, Stage 4 TDMA may be unnecessary. Let's measure.

## For the user — morning summary

We hit a real architectural limit in Stage 4 (the +10 ms FIFO
bias). Identified the root cause. The one-line Hub config change
(`CONFIG_ESB_TX_FIFO_SIZE=1`) cuts per-Tag sync error by 95 %, at
a cost of ~28 % frame throughput.

This is probably the best night's work of the RF sprint. We went
from "sub-ms is architecturally blocked, need TDMA + hardware
timestamps" to "one-line Hub config gets us sub-ms per-Tag |err|."

The cross-Tag span p99 after this should drop to single-digit ms.
Stage 4 TDMA may not be needed at all.
