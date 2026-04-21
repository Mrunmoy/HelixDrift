# RF closeout handoff

**Branch:** `nrf-xiao-nrf52840`
**Baseline commit:** `b112c2f` (tag `ota-reliable-v1`)
**Latest:** `810454b` (RF design brief)
**Last updated:** 2026-04-22

This is the **one document** any new agent (or any future-me) should
read to pick up RF work on HelixDrift. It captures what we've done,
what we've measured, what we've proven, what we haven't, what's
known-broken, and the design decisions that need to be made.

For the ground-truth wire protocol see
[`RF_PROTOCOL_REFERENCE.md`](RF_PROTOCOL_REFERENCE.md); for the
Hub-relay OTA path see [`NRF_HUB_RELAY_OTA.md`](NRF_HUB_RELAY_OTA.md);
for the most recent group review of where to go next see
[`RF_NEXT_STEPS_DESIGN_BRIEF.md`](RF_NEXT_STEPS_DESIGN_BRIEF.md) and
the two reviewer responses in `docs/reviews/`.

## 1. What works, proven

### 1.1 Hub-relay OTA (tag `ota-reliable-v1`)

Single firmware binary deployed across 10 Tags, each with a
flash-provisioned `node_id` at `0xFE000`. OTA from PC → USB → Hub →
BLE → Tag verified **100 % on 10-Tag fleet** in two consecutive 10×
runs. ~240 s per Tag sequential. Migration-round flakes resolved by a
4 s pre-`bt_enable` LFRC settle + per-chunk DATA_RSP back-pressure +
ATT MTU auto-exchange. See
[`NRF_HUB_RELAY_OTA.md`](NRF_HUB_RELAY_OTA.md) for the full fix list.

**The OTA path is a solved problem.** Don't touch it for RF work.

### 1.2 ESB steady-state throughput

After Phase C hardening (commit `eaeb79a` — per-Tag staggered
`retransmit_delay = 600 + 50·node_id µs`, `retransmit_count` 6→10,
boot-time TX phase offset `200·node_id µs`):

| Metric | Value |
|---|---:|
| Per-Tag mean (9 healthy Tags × 10 min) | **48.5 Hz** (target 50 Hz) |
| Combined throughput | **436 Hz** of 500 nominal (87 %) |
| Worst-case Tag rate | 45.5 Hz (Tag 2) |
| Best-case Tag rate | 51.2 Hz (Tag 9) |

Pre-Phase-C baseline was 41.8 Hz mean / 399 Hz combined. Phase C
bought +16 % per-Tag and +9 % combined.

### 1.3 Hub-reset recovery (Phase B + D)

Hub reset injected mid-stream causes a uniform **~2 s dropout** per
Tag then immediate resume at pre-fault rate. **Repeatable 4× in a row
with no cumulative degradation** (`rf_phased_multireset_*.csv`).
Robust capture tool (`tools/analysis/capture_mocap_bridge_robust.py`)
survives the CDC disconnect.

### 1.4 Flash-provisioned Tag node_id

Each Tag's identity is a single byte at `0xFE000` (first word of the
`settings_storage` partition, otherwise unused since
`CONFIG_SETTINGS=n`). OTA never touches this page, so node_id
survives every firmware roll. Sentinel `0xFFFFFFFF` means
unprovisioned → falls back to Kconfig default. Provisioning is a
two-line `nrfutil device program` + `nrfutil device write` recipe —
see [`NRF_HUB_RELAY_OTA.md`](NRF_HUB_RELAY_OTA.md) and the scratch
helper `/tmp/helix_tag_log/flash_tag.sh`.

## 2. What's "working" with known caveats

### 2.1 Cross-Tag sync: 19 ms median / 50 ms p99 (real metric)

The old `SYNC_SPAN_US` number in `capture_mocap_bridge_window.py`
(p99 2.1 s / max 44 s) was a **measurement artefact** — running max-
min across each Tag's *last-seen* `sync_us`, which blows up if any
Tag stops transmitting (Tag 5 broke it post-Phase-C).

Correct metric (`tools/analysis/sync_error_analysis.py`, bin-based,
stale Tags excluded):

| Metric | p50 | p99 | Max |
|---|---:|---:|---:|
| Cross-Tag span (50 ms wall bins, ≥ 5 Tags/bin) | **19 ms** | **50 ms** | 64 ms |
| Per-Tag `|sync_us - rx_us|` | 13 ms | 22 ms | 30 ms |
| Per-Tag mean bias | **-14 ms** (very tight: ±0.5 ms across 9 Tags) | | |

**19 ms median is usable for slow mocap** (rehab, VR avatar, film
preview with low peak angular velocity). **50 ms p99 is marginal for
fast mocap** (dance, sports, martial arts — 2.5-frame jitter at 50
Hz → ~20° joint-angle error at 400 deg/s peak). The user's answer
to Q1 of the design brief is that **both use cases are on the road-
map**, so this needs fixing before a fast-motion demo.

### 2.2 The -14 ms systematic bias — root cause

The estimator in `node_handle_anchor()`:
```c
estimated_offset_us = (int32_t)(local_us - anchor->central_timestamp_us);
```
bakes the whole "Hub RX → anchor reaches Tag" latency into the
offset. That latency depends entirely on whether `esb_write_payload`
made or missed the ACK-payload TIFS window:

- **Fast path:** anchor payload queued within ~150 µs TIFS after
  Hub's RX → anchor rides *this frame's* ACK → Tag RX ~500 µs after
  TX. Latency baked-in: ~500 µs.
- **Slow path:** `esb_write_payload` missed TIFS → payload stays in
  the **per-pipe ACK FIFO** → anchor rides the **next** successful
  RX's ACK (from this pipe). Latency baked-in: full 20 ms TX period.

Observed mean bias -14 ms ≈ 70 % slow path × 20 ms + 30 % fast path
× 0.5 ms. Matches.

### 2.3 Tag 5 hardware-dead

`HTag-0126`, node_id 5, FICR `15832601`. Already the weakest Tag in
the pre-Phase-C baseline (33 Hz vs. 42 avg). Post-Phase-C deployment
it went to 0.01 Hz across two consecutive 10-min captures and
survived a full reset. Uncorrelated with firmware changes.

Suspected hardware fault — possibly charge-circuit (TP4054), possibly
USB-seating, possibly solder issue. User has the TP4054 datasheet and
the ProPico schematics in `datasheets/` — those were added during a
previous Tag 1 blue-LED debug pass. Tag 5 likely needs the same
bench-level diagnosis.

**Until Tag 5 is repaired, all "fleet" conclusions are on 9 Tags,
not 10.** All measurements in this doc exclude Tag 5 explicitly.

### 2.4 Tag-side ESB retransmit tuning

Node firmware currently sets:
```
config.retransmit_delay = 600 + 50·node_id µs      (650..1100 µs)
config.retransmit_count = 10
```
This is the Phase C collision-hardening change. Do not revert
without remeasuring (+16 % per-Tag came from this).

## 3. What's broken / undocumented / deferred

### 3.1 Anchor-to-Tag association on shared pipe 0 (the deep problem)

**The nRF ESB PRX ACK-payload FIFO is per-pipe, not per-Tag.** When
Hub queues `esb_write_payload(anchor_for_Tag_A)` and misses the TIFS
window on Tag A's frame, that payload stays in the pipe 0 FIFO. The
next *any-Tag* frame that arrives on pipe 0 triggers an ACK — and
takes that queued payload.

So on the slow path (~70 % of anchors today), the anchor built from
**Tag A's RX timestamp** is delivered to **Tag B**. Tag B reads
`central_timestamp_us` and thinks "this is Hub's RX time of *my*
frame" — but it's Hub's RX time of Tag A's frame. The systematic
bias isn't per-Tag uniform because of this; it's actually bias +
inter-Tag-cross-contamination averaged to look uniform.

Two orthogonal fixes:

1. **Add `rx_node_id` to the anchor.** Hub stamps which Tag the
   anchor was built for. Receiving Tag filters: "if this anchor
   wasn't built for me, ignore its sync fields." Non-target Tags
   lose those anchor slots (and have to wait for the next
   fast-path-hit anchor), but at least the sync math stops
   being cross-contaminated.
2. **Per-Tag pipes (Phase E Option A promoted).** Split 10 Tags
   across multiple ESB pipes. 8 pipes max, so e.g. pipes 0..4
   with 2 Tags each, or pipes 0..7 with 1-2 Tags each. Each pipe's
   ACK FIFO is independent → anchor generated for Tag A on pipe N
   can only ever be delivered to Tags on pipe N.

Approach 1 is simpler (1 byte of wire, no per-pipe provisioning).
Approach 2 fixes both this issue AND gives multi-Hub room isolation
as a by-product, but doubles the per-Tag flash provisioning story
(need to know your pipe).

**This decision is currently open** — see section 5 for the group's
second-round opinion.

### 3.2 Anchor v4 as originally scoped is not sufficient alone

The brief (`RF_NEXT_STEPS_DESIGN_BRIEF.md` §3.1) proposed anchor v4
= add `anchor_tx_us` + midpoint estimator on Tag. That fix
addresses the *latency bias* but not the *cross-contamination*. On
shared pipe 0 in slow path, even if Tag knows exactly when Hub TXed
the anchor, the anchor's `central_timestamp_us` is still wrong for
that Tag.

**Fix the association problem first (§3.1 options) before — or
together with — anchor v4.** Anchor v4 on a correctly-associated
anchor is the right endgame.

### 3.3 No hardware TX-done timestamp in NCS v3.2.4 ESB

`struct esb_evt` fields on TX_SUCCESS: `{evt_id, tx_attempts}`. No
timestamp. The softest approximation is `now_us()` inside our
event_handler callback, which runs after the ESB event-queue ISR
dispatch. Expected jitter: sub-ms (ISR latency on nRF52840 at 64 MHz
is 5-30 µs typically, but our handler is not interrupt-priority 0).

For µs-precise TX time, bypass the driver and capture TIMER0 on
RADIO EVENTS_END via PPI. Out of scope for a 14 ms bias fix.

### 3.4 Untested failure modes

Per both reviewers (`docs/reviews/...`), we haven't tested:

- **Multi-hour soak** (longest capture was 10 min)
- **Thermal drift** (die heats up over hours, LF clock aging)
- **2.4 GHz coexistence** (WiFi router, BT headphones, LTE-U)
- **Battery sag / brownout** — Tags run off USB today; LiPo 4.2→3.2 V
  will change TX power, ESB RF behaviour, clock accuracy
- **Charge-while-streaming** behaviour (TP4054 charging interacts
  with 2.4 GHz?)
- **Body shadowing** — benchtop stacked on a desk ≠ worn on limbs
- **Repeated ESB ↔ BLE transitions** over hours (OTA-heavy flows)
- **10-healthy-Tag rerun** once Tag 5 is repaired — current data is
  9-Tag only
- **Tag mid-stream reset recovery** — only Hub reset tested

Any of these could surface a regression that our current measurements
don't see. The soak test is the single biggest blind-spot — schedule
a 2-4 h run before declaring "rock-solid."

### 3.5 Sync-span metric was lying; old tool still exists

`tools/analysis/capture_mocap_bridge_window.py` reports
`SYNC_SPAN_US` computed from last-seen values. **Treat that field
as obsolete.** Use
`tools/analysis/sync_error_analysis.py` on the CSV instead; that
tool uses wall-clock binning and stale-Tag exclusion. Did not
delete the old tool because its main counters (per-Tag frame count,
gap totals) are still useful.

## 4. Measurement methodology (what the metrics mean)

### 4.1 Capture tools

| Tool | Purpose |
|---|---|
| `tools/analysis/capture_mocap_bridge_window.py` | Original capture. Used for existing Phase A baselines. SYNC_SPAN field is misleading (see §3.5). |
| `tools/analysis/capture_mocap_bridge_robust.py` | Resilient variant. Reopens the CDC port if Hub reboots. Required for any test that injects Hub resets. Adds `DISCONNECT_EVENTS` to the summary. |
| `tools/analysis/sync_error_analysis.py` | Post-process a CSV. Emits per-Tag bias + bin-based cross-Tag span, with `--exclude <node>` for killing stale Tags. |
| `tools/analysis/analyse_mocap_gaps.py` | Post-process. Per-Tag largest-gap finder. Useful after fault injection. |
| `tools/analysis/log_mocap_bridge.py` | Pre-existing simpler logger. Use for ad-hoc live monitoring. |

All three new tools were upstreamed from `/tmp/helix_tag_log/` in
commit `7530732` — the `/tmp/` copies still exist for scratch use
but **do not edit them** — edit `tools/analysis/` and re-run.

### 4.2 Harness shell scripts (still in `/tmp/`)

| Script | Purpose |
|---|---|
| `/tmp/helix_tag_log/rf_baseline.sh` | 10-min baseline on 10 Tags |
| `/tmp/helix_tag_log/rf_phase_b_robust.sh` | Mid-stream Hub reset injection |
| `/tmp/helix_tag_log/rf_phase_d_multireset.sh` | 4× Hub reset stress in 10 min |
| `/tmp/helix_tag_log/flash_tag.sh` | SWD-flash + provision Tag `node_id` |
| `/tmp/helix_tag_log/fleet_ota.sh` | Round-based Hub-relay OTA harness |

These are scratch-only. If you rely on any of them for repeatable
measurement, **upstream them to `tools/nrf/`** first. (Both reviewers
flagged this as a risk — one `/tmp` flush kills measurement
repeatability.)

### 4.3 What "rate", "gap", "sync span" actually mean

- **Rate Hz** (per Tag) = frames received by Hub / capture seconds.
  Tag's nominal is 50 Hz (CONFIG_HELIX_MOCAP_SEND_PERIOD_MS=20). Any
  number under 50 is TX loss — collisions, retransmit exhaustion,
  or Tag not streaming (e.g. in OTA boot window for first 300 s
  after reset).
- **gap_per_1k** (per Tag) = total sequence-number gap / received
  frames × 1000. Uses the u8 sequence in the frame so wraps at 256;
  big numbers mean Tag is consistently missing frames between the
  ones that land. Not very useful in absolute terms; useful for
  A/B deltas.
- **Cross-Tag span** (bin-based, new) = for each 50 ms wall-clock bin
  where ≥ 5 Tags reported a frame, `max(sync_us) - min(sync_us)` in
  that bin. Distribution across bins is what you report as p50/p99.
- **Per-Tag bias** = `mean(sync_us - rx_us)` for that Tag's frames.
  Currently -14 ms uniform across the 9 healthy Tags; see §2.2.

## 5. Group review — decisions and open questions

### 5.1 Outputs already on file

- [`RF_NEXT_STEPS_DESIGN_BRIEF.md`](RF_NEXT_STEPS_DESIGN_BRIEF.md) —
  the brief that kicked off group review
- `docs/reviews/copilot_2026-04-21.md` — Copilot's round-1 response
  (protocol / RF / product sub-agent decomposition)
- `docs/reviews/codex_2026-04-21.md` — Codex's round-1 response
- `docs/reviews/*_followup.md` — round-2 follow-up on the
  "per-pipe vs rx_node_id" question (in-flight at time of writing;
  replace this bullet with filenames once landed)

### 5.2 Consensus from round 1

Both reviewers converged on:

1. **Do Anchor v4, but rescope it — the brief's half-day estimate
   understates the work.** Protocol reviewer flagged the
   ACK-per-pipe-FIFO issue. `anchor_tx_us` alone doesn't fix it.
   Revised scope: 1-1.5 days including the association fix.
2. **Phase E Option A (flash-provisioned Hub pipe address)** is
   cheap (~2 h) and architecturally agreed. Do it even if
   multi-Hub isn't on the roadmap, because it prevents physical
   re-provisioning debt later.
3. **"Rock-solid" overstates reality.** Soak, coexistence,
   battery-sag, thermal all untested.
4. **Tag 5 is not a firmware issue.** Physical inspection required.
5. **Commit the `/tmp/` tooling** before any further pivoting.
   (Done in `7530732`.)

### 5.3 Open decisions

**Decision D1 — How to fix anchor-Tag association on shared pipe 0?**
(Section 3.1 above.) Options: `rx_node_id` in anchor, per-Tag pipes
via Phase E Option A promotion, or both. Second-round review in
flight.

**Decision D2 — Ship anchor v4 in a wire-format bump, or defer?**
Depends on D1. If we pick "per-Tag pipes" we can keep v3 wire
format and just change addressing. If we pick "rx_node_id" we need
wire format v4.

**Decision D3 — Where does parallel OTA sit on the RF roadmap?**
Both reviewers punted to "after RF is done, only if OTA time is
actually hurting." User de-prioritised in the original parking of
task #21.

## 6. Roadmap (recommendation as of this writing)

Strictly ordered:

1. **Identify Tag 5 physically** (user action, takes 5 min).
2. **Commit `/tmp/` harness scripts into `tools/nrf/`** (mine, 30 min).
3. **Hub-side counters for fast-path vs slow-path anchors** —
   instrumentation before making the D1 decision (~1 h). Answers
   "what fraction of anchors are slow-path?" with hard data.
4. **Per-Tag `sync_error_us` + `frames_since_last_rx` in Hub USB
   output** — both reviewers asked. Needed so the PC app layer
   (when built) can alert on RF degradation without post-hoc
   analysis. ~1 h.
5. **Land D1 decision (anchor-Tag association fix).** Implement
   it. ~1-1.5 days including Phase E Option A prerequisite if we
   pick per-Tag pipes.
6. **Anchor v4 with midpoint estimator** once D1 is in. ~4 h.
7. **Long-duration soak test** (2-4 h run, fault-injection
   harness). Validates steady-state + recovery across hours.
8. **2.4 GHz coexistence test** (Wi-Fi router + BT headphones in
   room). Validates real-world environments.
9. (Only now consider) RF work done. Move to PC-side fusion.

Steps 1-4 are quick wins. Steps 5-8 together are the "real RF
closeout" — 3-4 focused days.

## 7. Commit chain (recent)

```
c9de886  m8-rf: overnight RF robustness report                 (docs)
eaeb79a  m8-rf: Phase C collision hardening                    (firmware)
7530732  m8-rf: sync-metric fix + upstream measurement tooling (tools/docs)
810454b  m8-rf: RF next-steps design brief                     (docs)
b112c2f  m8: Hub-relay OTA 5/5 reliable (tag ota-reliable-v1)  (firmware)
ffff061  m8: apply Copilot OTA code review fixes               (firmware)
e6b0f17  m8: flash-backed Tag node_id                          (firmware)
f2fd101  m8: ESB OTA trigger flood fix                         (firmware)
b35bab5  m8: 4s LFRC settle + pre-InfoRsp ordering             (firmware)
b8201fe  m8: MTU + DATA_RSP ack                                (firmware)
fee0b50  m8: Hub relay OTA works — write-with-response         (firmware)
```

The tag `ota-reliable-v1` at `b112c2f` is the "known good OTA" anchor.
Anything from `eaeb79a` onwards adds RF-side improvements; anything
before only affects OTA. If you need to roll back an RF change
without touching OTA, reset to `ota-reliable-v1` and cherry-pick
the OTA-related commits forward.

## 8. Hardware / test-rig notes

- **Hub** is an nRF52840 dongle on J-Link SN `69656876`,
  enumerates as `/dev/ttyACM1` (USB CDC).
- **Tag 1** is an nRF52840 ProPico, attached to J-Link SN `123456`
  (J-Link Plus external). It's the only Tag with SWD access right
  now — the other 9 are USB-powered only.
- Tags 1..10 have node_ids 1..10 respectively (see
  `/tmp/helix_tag_log/programmed.txt` for the full FICR → node_id
  table).
- **All 10 Tags are on firmware v12** (Phase C). Do not OTA further
  without reading §3 of this doc — in particular §3.1 — first.
- **Tag 5 (`HTag-0126`, FICR `15832601`)** is hardware-flatlined.
  Physical inspection pending.

## 9. Questions nobody has answered yet

- What peak angular velocity does the v1 deployment need? We've
  assumed fast motion is on-roadmap (per user Q1 answer) but
  haven't quantified the requirement. Determines whether 50 ms p99
  span is a hard fail or merely ugly.
- Does the ProPico board's 32 kHz LFCLK source (XTAL per board DTS)
  match what's actually populated on these specific boards? If any
  board is falling back to LFRC without us noticing, its sync
  characteristics would be wildly different.
- Battery life at 50 Hz ESB? Product requirements doc cites "> 4 h"
  but there's zero measurement behind it.
- Antenna / physical layout: all 10 Tags on the bench are in
  line-of-sight of the Hub. Real deployment will not be. Need a
  "wear on a body" measurement pass before shipping.
