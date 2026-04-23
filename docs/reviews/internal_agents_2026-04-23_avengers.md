# Internal agents — Avengers brainstorm (2026-04-23)

Two internal Claude subagents consulted alongside Codex + Copilot
for the 17-Tag × 200+ Hz × sub-ms architectural question. Both
independent; neither saw the other's output.

---

## Panel 1 — general-purpose agent (simulated virtual team)

Agent assembled a 5-person virtual team (Radio / Protocol / Mocap /
Hardware / Product) and deliberated.

### Top-line

Unanimous: **nRF52840 + ESB is a dead end for the full-product
target.** Two-SKU product off one reference design:

| | **Studio** (tier 1) | **Pro** (tier 2) |
|---|---|---|
| Tags | 17 × nRF54L15 + ICM-42688 + 150 mAh LiPo | same + DW3000 UWB |
| Hub | nRF54L15 + USB-C | + DW3000 + Ethernet PoE |
| Radio | BLE 5.4 ISO (BIS) | BIS data + UWB sync |
| Rate | 200 Hz | 500 Hz |
| Cross-Tag sync | ~1 ms | < 100 µs |
| BOM/Tag | $28–35 | $42–48 |
| Battery | 6 h | 4 h |
| Target | Rokoko Smartsuit tier | Xsens MVN Link tier |

### Radio choice verdict table (verbatim)

- **nRF54L15 + BLE CIS/BIS**: Strong candidate — 128 MHz Cortex-M33,
  faster radio, better PPI/DPPI, Zephyr-native, drop-in-ish upgrade.
- **802.15.4 TSCH on nRF52840**: No — 250 kbps PHY insufficient at
  17 × 200 Hz.
- **UWB (DW3000/Qorvo)**: Hybrid use only — great for sub-ns time
  sync (~100 ps TWR), inadequate for data streaming.
- **WiFi 6 + TWT (ESP32-C6)**: Dark horse — PHY fits, power is
  3-5× ESB, field support nightmare.
- **Hybrid UWB-sync + BLE-data**: Winner for fast-motion tier.
  Matches the architecture of Rokoko Coil Pro.

### Migration plan

- **Phase A (0–1 mo):** Ship current state as v1.0 on ESB, tagged
  rehab/VR avatar product. Don't sink more into ESB.
- **Phase B (1–3 mo):** nRF54L15 + BLE CIS bring-up on 3-Tag rig,
  parallel to v1.0 shipping. Port platform-agnostic
  `firmware/common/` + `simulators/` forward.
- **Phase C (3–6 mo):** 17-Tag nRF54L15 BIS fleet with hardware TX
  timestamping from day 1.
- **Phase D (6–9 mo optional):** Add UWB daughter for sports SKU.

### Disagreements surfaced

- RE vs HW on single-radio (CIS) vs hybrid (UWB+BLE) — resolved
  by tiered SKU.
- PA vs PO on whether to ship v1.0 on ESB — resolved: yes,
  freeze branch then build new radio stack alongside.
- MDE reservation: 200 Hz may not be enough for karate ankle; Pro
  SKU at 500 Hz non-negotiable for sports claim.

---

## Panel 2 — hardware-tester agent

Hardware-reality check. Opinionated hardware-engineer POV.

### IMU: ICM-42688-P is the only serious choice

- Gyro noise density 2.8 mdps/√Hz (BMI270: 8 mdps/√Hz) — 3× better
- ±4000 dps full-scale — BMI270 tops out at ±2000 dps, **will clip
  on judo throw hip rotations**
- 2 KB FIFO with timestamping — critical for sync story
- ~$4.50 qty 1k

### Tag PCB BOM (nRF54L15 + ICM-42688) — target $32–38 @ 1k

| Block | Part | Notes |
|---|---|---|
| MCU+Radio | nRF54L15 QFAA | $4.80 |
| IMU | ICM-42688-P | $4.50 |
| Antenna | Ignion NN03-310 or Abracon ARFC chip antenna | Pre-tuned; do not roll a PIFA |
| PMIC | Nordic nPM1100 | $1.20, buck 92% eff beats LDO 45% |
| Battery | 110 / 250 mAh LiPo | per zone |
| Crystal | 32.768 kHz xtal | Non-negotiable for BLE CIS sync |
| Misc | LDO for IMU analog, load switch, USB-C | ~$3 |

### Form factor — 2 PCB variants, not 17

| Zone | Count | Target | Battery |
|---|---:|---|---|
| Finger/hand | 6 | <8g, <3cm³ | 50-80 mAh |
| Limb | 9 | <15g, <6cm³ | 150 mAh |
| Torso/pelvis/head | 2 | <25g | 250-400 mAh |

### Test rig for validation

- **OptiTrack Prime 13** 6-cam setup ~$25k — minimum ground truth
- **Rotary encoder on motor pendulum** ~$800 — repeatable lab motion
- **Common photodiode LED trigger** on every Tag — hard ground-truth
  sync event independent of radio
- **Minimum lab capex: ~$30k**

### Final sync verdict

BLE 5.4 CIS **spec-sheet** says 100 µs sync; in a 17-Tag dense
piconet with body-worn antennas (10-15 dB absorption at 2.4 GHz),
real-world is **300-800 µs — right at sub-ms ceiling, no margin**.

UWB DW3000 at $6.50 gives 100 ns ranging+timing. Recommendation:
Build nRF54L15-only first, characterize. If real-world slips past
500 µs on body, respin with DW3000. **PCB land pattern should be
pre-reserved.**
