# HelixDrift product-architecture roadmap (2026-04-23)

**Status:** Synthesised from 4-panel Avengers brainstorm — internal
general-purpose agent, internal hardware-tester agent, Codex, Copilot.
All four raw responses persisted in `docs/reviews/`:
- `avengers_brainstorm_prompt_2026-04-23.md` (the ask)
- `internal_agents_2026-04-23_avengers.md` (both internal panels)
- `codex_2026-04-23_avengers.md`
- `copilot_2026-04-23_avengers.md`

**Ambition:** Full-body wearable IMU mocap — 17 Tags, 200+ Hz per
Tag, sub-ms cross-Tag sync. Use cases include karate / judo /
somersaulting (2000 °/s peak angular velocity, 100 ms strike
durations). Comparable to Xsens MVN / Rokoko Smartsuit / Noitom
Perception Neuron.

## Headline — unanimous across all four panels

**nRF52840 + Nordic ESB is a dead-end for the full-product target.**
Every firmware experiment this sprint refuted its hypothesis.
Cross-Tag span p99 is pinned at ~42 ms by architectural choices —
shared pipe 0 + ACK-payload sync + software timestamps — none of
which can scale to 17 Tags × 200 Hz × sub-ms.

**Physics check (Copilot):**
- 200 Hz × 17 Tags **bidirectional** ESB: 157 % duty cycle — impossible
- 200 Hz × 17 Tags **unidirectional TDMA, no-ACK**: 72 % duty cycle — feasible
- 500 Hz × 17 Tags unidirectional TDMA: 97.6 % duty cycle — at physical limit

The architectural fix is therefore **uniform across panels**:
- **TDMA slots** with hub-broadcast beacon (not ACK-payload midpoint)
- **Hardware TX + RX timestamps** via PPI + TIMER (not software `now_us()`)
- **PC-side clock authority** (not Tag-side estimator)

The disagreements are about **which silicon** and **whether UWB**.

## Where the panels converge

### IMU upgrade — ICM-42688-P (hardware-tester + Copilot)
- Gyro noise density 2.8 mdps/√Hz (vs BMI270's 8 mdps/√Hz)
- ±4000 dps full-scale (BMI270 ±2000 dps will clip on judo hip rotation)
- 32 kHz internal ODR, 2 KB FIFO with HW timestamping
- ~$4.50 at 1k volume
- **Current LSM6DSO upgrade path is clean** — existing sensor driver
  abstraction supports replacement.

### Sync architecture — Hub beacon + no-ACK TDMA + HW timestamps
Copilot laid out the canonical design:
```
Hub TX:  Beacon on ESB pipe 1, broadcast address, every 5 ms cycle.
         Payload: hub_hw_ts (PPI-captured @ RADIO.EVENTS_TXREADY).
         All 17 Tags RX the same beacon simultaneously.

Tag TX:  ESB pipe 0, TDMA slot, NO ACK.
         Payload: node_id + tag_hw_ts + quaternion + seq.

Hub RX:  PPI → TIMER CC capture @ RADIO.EVENTS_END = hub_rx_hw_ts.
         Hub emits to PC: {node_id, tag_hw_ts, hub_rx_hw_ts, quat, seq}.

PC:      offset_tag[i] = hub_rx_hw_ts - tag_hw_ts - propagation_ns
         (propagation ≈ 1 ns/cm × 30 cm = 1 ns, negligible).
         PC aligns all Tag streams to hub_rx_hw_ts.
```

Expected cross-Tag span p99 after this: **< 200 µs** (limited by
TIMER resolution + ~50 ppm clock stability + slot jitter). 200× better
than today.

### Code / assets to keep
All four panels agree these survive any radio migration:
- **SensorFusion AHRS** (external/SensorFusion submodule)
- **Sensor drivers** (firmware/common)
- **Hub-relay BLE OTA** (proven 100 % reliable, don't touch)
- **Flash-provisioned `node_id` at 0xFE000**
- **Python analysis tooling** (`tools/analysis/`)
- **Host simulation harness** (simulators/)
- **Wire format skeleton** (extended with HW timestamp fields)
- **MCUboot OVERWRITE_ONLY** config
- **`fleet_ota.sh`** with the recent hardening

### Migration style — radio/MAC replacement, not full rewrite
All four panels: **build new radio stack alongside, not inside**.
`firmware/common/` and `simulators/` are platform-agnostic and
survive. Only `zephyr_apps/nrf52840-mocap-bridge/` (the MAC-heavy
layer) gets replaced. Keep ESB v22 as legacy branch reference.

## Where the panels disagree — three open decisions

### D-A: Silicon — nRF52840 or nRF54L15?

| Panel | Position | Reasoning |
|---|---|---|
| **Agent** | nRF54L15 | 128 MHz Cortex-M33, 3.5× faster radio, better PPI/DPPI, lower power; Zephyr-native, drop-in-ish |
| **Hardware-tester** | nRF54L15 | Same; dev kit schematic ships Q2 2026 |
| **Codex** | nRF54L for clean slate; nRF52 viable for prototype | "Real win, not a miracle" |
| **Copilot** | **Keep nRF52840 for 200 Hz**; move to nRF5340 only if 500 Hz is mandated | CPU/RAM/flash analysis shows nRF52840 is adequate at 200 Hz; ESB library supports no-ACK + PPI pattern; BOM $4.00 vs nRF54L15 $4.80; OTA path is proven |

**Synthesis:** Copilot's view is more conservative but has the
strongest physics+BOM argument for 200 Hz. Agent/Codex prefer
nRF54L15 for cleaner hardware (DPPI, lower power) and forward
compatibility with BLE ISO. **Decision deferrable** — the TDMA
redesign doesn't lock in silicon choice until PCB respin.

**Recommendation:** Prototype TDMA design on nRF52840 first (zero
hardware change, use existing fleet). If prototype shows <1 ms
cross-Tag span p99 and meets power budget, ship on nRF52840.
Migrate to nRF54L15 at the next PCB respin (likely for IMU swap
to ICM-42688-P anyway).

### D-B: UWB — default architecture or tier-2 only?

| Panel | Position | Reasoning |
|---|---|---|
| **Agent** | Tier-2 only (optional Pro SKU) | UWB is overkill for data; add as sync plane on sports SKU |
| **Hardware-tester** | Reserve PCB land, add if characterization slips | BLE CIS real-world 300-800 µs on body is "right at ceiling, no margin" |
| **Codex** | Clean-slate has UWB; simpler first product uses multi-Hub Nordic | "Sync is sacred → UWB-authoritative; otherwise multi-Hub proprietary" |
| **Copilot** | Not emphasised — focuses on TDMA without UWB | nRF52 physics alone fits 200 Hz |

**Synthesis:** UWB is the safest path to **guaranteed sub-ms cross-Tag
sync** at 17 Tags. Without UWB, the system relies on excellent nRF radio
+ antenna design to stay under 1 ms in real body-worn conditions. Both
hardware-tester and Agent's "reserve land, build without, add if
needed" approach is the lowest-risk path.

**Recommendation:** Pre-reserve DW3000 PCB footprint on the Tag.
Build first silicon without UWB, characterize on body. If p99 > 500 µs
at 17 Tags, respin with UWB. DW3000 costs ~$6.50 — small BOM hit,
huge margin insurance.

### D-C: 500 Hz — baseline requirement or stretch goal?

| Panel | Position | Reasoning |
|---|---|---|
| **Agent** | 500 Hz required for sports / martial arts tier (Pro SKU) | 2000 °/s ankle kicks need temporal resolution |
| **Mocap domain expert (via Agent)** | "200 Hz may not be enough for karate ankle; Pro SKU at 500 Hz is non-negotiable" | Same |
| **Copilot** | 200 Hz baseline, 500 Hz stretch | "Sense locally at 500 Hz, stream 200 Hz live, add burst modes only if measurement proves need" |
| **Hardware-tester** | 100 Hz on fingers, 500 Hz on limbs/torso | Defensible product tiering |

**Synthesis:** Local 500 Hz sensing (ICM-42688-P has 32 kHz ODR
internally) is cheap; the contention is **live streaming rate**.
Copilot's "stream 200 Hz, burst-mode for on-demand high-rate
capture" is elegant and maps cleanly to how pro mocap is used
(continuous for coaching feedback, on-demand for analysis).

**Recommendation:** Ship 200 Hz live stream with local 500 Hz
FIFO on Tag. Add a "burst mode" command that temporarily uplinks
500 Hz during a defined capture window (say 10 seconds). Requires
on-Tag FIFO buffering during burst and back-fill streaming.

## Proposed product tiering

Reconciling Agent's two-SKU proposal with Copilot's single-product
view via **one hardware, two feature tiers**:

| | **Studio** | **Pro** (field upgrade or SKU) |
|---|---|---|
| **Hardware** | nRF52840/54L15 + ICM-42688-P + nPM1100 | Same + DW3000 UWB daughterboard or Tag respin |
| **Radio** | Custom TDMA on ESB pipe 0 + beacon on pipe 1 | Same + UWB sync plane |
| **Live rate** | 200 Hz | 200 Hz live / 500 Hz burst |
| **Cross-Tag p99** | < 1 ms (target) | < 100 µs |
| **Use case** | Rehab, VR avatar, film previz, dance | Sports, martial arts, biomech research |
| **BOM / Tag** | $18–22 | $25–30 |
| **Battery life** | 6 h | 4 h |
| **Target market** | Rokoko Smartsuit tier ($2k price point) | Xsens MVN Link tier ($6k+) |
| **When** | Phase C (3–6 months from v1.0 ship) | Phase D (6–9 months) |

**Both tiers use the same firmware codebase with compile-time or
runtime feature flags.** Single Tag PCB design parameterised by
battery footprint (finger / limb / torso = 3 enclosures, 1-2 PCB
variants).

## Migration phases — consensus across panels

### Phase A — Freeze v1.0 on ESB (0–1 month)
- Ship `rf-sprint-checkpoint-v1` as **v1.0 rehab/VR product**.
- Spec: 10 Tags × 50 Hz, 42 ms cross-Tag p99.
- Merge `nrf-xiao-nrf52840` → `main`, tag `v1.0.0`.
- Cost: days. Revenue: validates product-market fit for the slow-motion
  use case before investing architecture-change capital.

### Phase B — TDMA + HW timestamp proof-of-concept (1–3 months)
- Stay on **nRF52840 silicon** (existing Tag fleet, no hardware change).
- Build new firmware tree `zephyr_apps/nrf52840-mocap-bridge-tdma/`
  alongside existing ESB app. Do NOT edit the ESB app in place.
- Replace ESB ACK-payload sync with:
  - Hub beacon on ESB pipe 1 (broadcast), 5 ms cycle.
  - Tag TDMA slot TX on pipe 0 with PPI-captured hw_ts.
  - Hub RX with PPI-captured hub_rx_hw_ts.
  - PC-side clock authority via new format.
- Validate on 3–8 Tag subset. Target: < 1 ms cross-Tag span p99.
- Cost: 2–4 weeks firmware + 1 week characterisation. No BOM.

### Phase C — 17-Tag fleet on TDMA (3–6 months)
- If Phase B hits target: proceed to fleet scale on same hardware.
- If Phase B slips > 500 µs: respin PCB with nRF54L15 + ICM-42688-P
  (and optionally reserved DW3000 footprint). Phase C delivers on
  new silicon.
- Includes: PC-side fusion pipeline using hw-timestamped streams,
  OSC / VRChat / MVN-compatible output format, manufacturing test
  fixtures.
- Cost: 6–10 weeks firmware + new PCB run.

### Phase D — UWB sports tier (6–9 months, optional)
- Add DW3000 sync plane for Pro SKU.
- Requires PCB respin if footprint wasn't reserved.
- Cost: 4–6 weeks.

## Outstanding questions (not yet answered by panels)

1. **Does BLE CIS/BIS outperform custom proprietary TDMA on nRF?**
   Agent strongly endorses BLE ISO; Codex explicitly says BLE ISO is
   NOT the obvious answer for 17-Tag deterministic uplink; Copilot
   bypasses the question by recommending custom TDMA on ESB. Open
   question for when we get to the actual Phase B / C design.
2. **How bad is 2.4 GHz body absorption at 17 distributed Tags?**
   Hardware-tester: 10–15 dB loss observed. Not yet measured on
   this fleet. On-body RF link budget test is a Phase B prerequisite.
3. **Does multi-Hub (Codex's rescue path) offer a better risk profile
   than single-Hub+UWB?** Two dongles at different RF channels, each
   handling 8 Tags, vs one dongle with UWB sync plane. Both are
   viable sub-ms paths.

## What this doc supersedes in RF.md §8

- **Bucket 1** (fleet hygiene) — still valid, track separately.
- **Bucket 2** (ship v1 at current spec) — becomes Phase A above.
- **Bucket 3** (architecture change for sub-ms) — expanded into
  Phase B–D. The "three options" in old §8 (3a hardware TX timestamp,
  3b per-Tag pipes, 3c TDMA with beacon) all appear in the Phase B
  design, executed together rather than picked between.

RF.md §8 will be updated to point to this roadmap.

## Next concrete action (user decision needed)

Pick one:

**Ship-first path** — commit to Phase A (merge to main, tag v1.0,
announce slow-mocap product), then start Phase B in parallel.

**Architecture-first path** — skip Phase A release, commit to
Phase B now on nRF52840. Validate TDMA + HW timestamps before
considering any v1.0 ship.

**Conservative** — do Phase A + Phase B in sequence on nRF52840.
Accepts 3–6 months before architecture result.

The technical risk is lowest for **Ship-first** because it
validates product demand before the architecture investment. The
team bias should be toward that unless market reasons override.
