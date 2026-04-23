AVENGERS BRAINSTORM — HelixDrift architecture for full-body mocap

Assemble your strongest possible team of experts. I need
rigorous, no-compromises architectural thinking. The product's
whole point is defeated if we accept limitations now.

## The ambition

Build a wearable IMU-based mocap system that works for:
- Running, walking, jumping — baseline cases
- Gymnastics, somersaulting — torso inversion, drift-correction hard
- Karate, judo, boxing — full-speed strikes, grappling, impacts

"Works for" means: avatar fidelity on a commercial mocap-suit tier,
comparable to Xsens MVN / Rokoko Smartsuit / Noitom Perception Neuron.

### Sensor count requirement

Full-body mocap in the fighting / gymnastics space uses
**17 IMUs** industry-standard (head, sternum, pelvis, 2 upper arms,
2 forearms, 2 hands, 2 upper legs, 2 lower legs, 2 feet). Possibly
19 with scapulae.

### Temporal requirements

- **Sample rate per Tag:** 200 Hz minimum for strikes. 500 Hz
  nice-to-have (research-grade). Peak angular velocity at ankle
  during a kick: ~2000 deg/s. At 50 Hz that's 40° between samples
  — unacceptable blur.
- **Cross-Tag sync span p99:** sub-ms. At 42 ms (our current
  ceiling), an 100 ms punch has 40% of motion temporally blurred.
- **Latency to host:** < 50 ms preferred for any real-time use
  case (live coaching, VR, streaming).

## Current state (see docs/RF.md for the full story)

We've spent a sprint on nRF52840 + ESB with a 10-Tag fleet on a
shared pipe 0. The architectural ceiling we hit:

- **Per-Tag |err| p99:** 6-10 ms (sub-ms p50) — good.
- **Cross-Tag span p99:** 42 ms — the wall.
- **TX rate:** 50 Hz per Tag, 430 Hz aggregate.

Two firmware-level experiments (v22 glitch-reject, offline drift
replay) both refuted their hypotheses — the fat-tail is
**per-Tag single-Tag tail events** (retry storms, radio glitches,
ISR jitter) that firmware estimators cannot suppress.

Architectural levers we identified (Bucket 3 in docs/RF.md §8):
- **Hardware TX timestamping** (PPI + TIMER capture on
  RADIO.EVENTS_END) — 3-5 days of firmware work
- **Per-Tag ESB pipes** — blocked at 8 Tags (hardware pipe limit)
- **TDMA with hardware Hub beacon** — 5-7 days, requires both above

None of these alone solve the **17-Tag × 200+ Hz × sub-ms** target.

## Questions for your team

### Part A — Can nRF52 do this at all?

A1. 17 Tags × 200 Hz × 32-byte payload = **108 kbps aggregate at
    application layer**. ESB at 2 Mbps has headroom, but only 8
    physical pipes and a single shared FIFO. Can we make 17 Tags
    work on nRF52 ESB realistically, or does this need a different
    radio/MAC?

A2. If ESB is out, what about:
    - **Nordic custom protocol** (Radio peripheral direct,
      bypass ESB). Used by 1Mbps proprietary protocols on
      nRF52/53.
    - **BLE Periodic Advertising** or **Isochronous Channels**
      (BIS/CIS) — Bluetooth 5.2+ has deterministic timing.
    - **IEEE 802.15.4 with TSCH** (Time-Slotted Channel Hopping).
      nRF52840 has 802.15.4 radio mode; OpenThread/Zigbee run it.
    - **Custom TDMA on raw Radio peripheral** with PPI-hardware
      TX scheduling.

A3. The nRF54L series (released 2024) has a different radio
    architecture — is there a win there without changing silicon?

A4. What about **multi-Hub** architecture? Split 17 Tags across
    2-3 Hubs, each on a different RF channel, PC aggregates.
    Trades RF scaling for more dongles but each Hub becomes
    tractable.

### Part B — If nRF52 can't do it, what chip/radio can?

B1. **UWB** (Decawave DWM3000 family, Apple U1, Qorvo) — native
    hardware TX timestamping at ns precision, designed for
    sync-critical positioning. Downside: different stack, higher
    cost per Tag.

B2. **WiFi 6 / WiFi Direct** with TWT (Target Wake Time)? 200 Hz
    per Tag × 17 Tags is trivial for WiFi bandwidth, and WiFi
    offers hardware timestamping.

B3. **Custom 2.4 GHz transceiver with dedicated sync-optimized
    MAC** (e.g., TI CC26x2 with TI-15.4, NXP Kinetis W series)?

B4. Is there a **hybrid** that keeps nRF52 for the Tag-side IMU
    proximity work but uses a different aggregation radio toward
    the Hub? E.g., tiny mesh network that hands off to a higher-
    bandwidth aggregation link.

### Part C — Timing architecture

C1. For sub-ms cross-Tag sync, where does the authoritative clock
    come from?
    - Hub-authoritative (our current approach)
    - GPS-disciplined oscillator on each Tag (impractical indoors)
    - Audio chirp sync beacon (acoustic, common in some mocap
      systems)
    - Hardware-level precision time protocol (PTP / IEEE 1588)
      over the radio link

C2. What's the minimum precision achievable on any of these?

C3. If we add **UWB pulse** for timing but keep nRF for data,
    does that work? (Ultraleap / some commercial suits use this
    pattern.)

### Part D — Rate scaling

D1. 200 Hz per Tag × 17 Tags = 3400 frames/sec aggregate. At
    32-byte payloads, that's 108 kbps — trivial. But the air
    medium has to carry each frame individually, with contention
    handling. What's the right MAC strategy?

D2. 500 Hz per Tag × 17 Tags = 8500 fps. At some point the radio
    PHY rate itself becomes the bottleneck. Where's the ceiling
    for 2.4 GHz ISM?

### Part E — Productization constraints

E1. Per-Tag cost target: we can probably swallow up to ~$30 BOM
    per Tag if it enables the target performance. nRF52840 is
    ~$5. UWB chips are ~$8-15. 17 Tags × $30 = $510 BOM —
    comparable to Rokoko Smartsuit pricing.

E2. Battery life per Tag target: 4+ hours continuous.

E3. Worn form factor: small, light. Current ProPico boards are
    ~1.2 cm × 1.8 cm — already good.

E4. Over-the-air update: already solved on nRF52 (Hub-relay OTA
    100% reliable). Would switching radios cost us this?

## Deliverables

From each team, I want:

1. **Honest assessment** of whether the target (17 Tags × 200 Hz
   × sub-ms cross-Tag × 4 h battery × ~$30 BOM per Tag) is
   achievable on **current hardware** (nRF52840), what it would
   take, and estimated effort.

2. **Clean-slate architectural proposal** — what's the right way
   to build this if we weren't constrained by our current
   codebase? Which radio, which MAC, which sync protocol, which
   topology?

3. **Pragmatic path** — IF our current nRF52 ESB work is a dead
   end for the full-product target, what's the minimum-risk
   migration? Do we need to blow up the firmware or can we build
   alongside and hand over?

4. **Specific product recommendation** — with budget $30-50/Tag
   BOM, 2026 timeline, 17-Tag target, what would your team ship?
   Be opinionated.

Format: structured by discipline (Radio / Protocol / Firmware /
Hardware / Product), single top-line recommendation at the very
top, then details. Disagreements between experts should be
surfaced, not smoothed. Under 1500 words total.

If you think the problem is infeasible as stated, say so — but
then tell me what's the closest-achievable product and why.
