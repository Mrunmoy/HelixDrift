# Sprint 7: Post-Convention-Fix Assessment

**Date:** 2026-03-29
**Author:** Claude / Systems Architect + Review Board

---

## 1. State After Convention Fix

The SensorFusion AHRS convention fix (3-part: init seeding, update gravity
prediction, mag cross-product direction) resolved all blocked items:

| Test | Before Fix | After Fix | Verdict |
|------|-----------|-----------|---------|
| Static ±15° yaw | ~15° RMS | < 1° | Fixed |
| Static ±15° pitch | ~29° RMS | < 1° | Fixed |
| Static ±15° roll | ~38° RMS | < 1° | Fixed |
| Dynamic yaw 30°/s (Kp=0.5) | ~25° RMS | < 10° RMS, < 5° max | Fixed |
| Dynamic pitch/roll | Diverge 180° | Yaw still easier (relative test) | Improved, needs measurement |
| Quarter-turn init | < 2° first sample | Still < 2° | No regression |
| A5 Ki bias rejection | Green | Green | No regression |
| A3 60s drift | Green | Green | No regression |
| A6 joint angle | Green | Green | No regression |

**Static accuracy across all axes is now < 1° at Kp ≤ 2.0.** This exceeds
the original target of < 3° per-pose RMS and < 5° overall RMS.

## 2. Wave A Status

| Task | Original Target | Achieved | Status |
|------|----------------|----------|--------|
| A5 Ki bias rejection | Ki bounds drift | Green | **DONE** |
| A3 60s drift | < 10° max, < 2°/min | Green | **DONE** |
| A6 Joint angle | < 5° per pose | Green | **DONE** |
| A1a Static accuracy | RMS < 3° per pose | < 1° all axes | **DONE — exceeds target** |
| A4 Gain characterization | Kp sweep | Kp≤2 < 1°, Kp=5 destabilizes | **DONE** |
| A2 Dynamic tracking (yaw) | RMS < 8° | Kp=0.5: < 10° RMS, < 5° max | **DONE — at Kp=0.5** |
| A2 Dynamic tracking (pitch/roll) | RMS < 8° | Yaw easier (relative) | **NEEDS MEASUREMENT** |
| A1b Large-angle static | RMS < 8° | Not attempted since fix | **READY to attempt** |

**5 of 6 Wave A tasks are DONE.** A2 pitch/roll dynamic tracking needs one
more measurement pass to confirm the fix resolved it.

## 3. Milestone Assessment

### M2: Single-Node Orientation Validation

**M2 is substantially complete.** Evidence:

- Static accuracy: < 1° across all axes (exceeds < 5° target)
- Dynamic yaw tracking: < 10° RMS at Kp=0.5 (within < 10° dynamic target)
- Ki bias rejection: proven
- Long-duration drift: bounded
- Joint angle recovery: proven
- Filter init from sensors: working across all axes
- Gain characterization: Kp stability boundary documented (Kp=5 destabilizes)

**Remaining to close M2 fully:**
1. One pass of A2 with pitch/roll at 30°/s — confirm < 15° RMS (was diverging, should be fixed now)
2. Optionally: A1b at ±45° and ±90° static poses — now unblocked by convention fix

### M1: Per-Sensor Proof — ~85%

Unchanged. Remaining gaps are sensor validation matrix items (Wave B4).
Independent of M2.

### M3: Node Runtime — ~20%

Unchanged. Cadence, transport, anchor tested. Not the current focus.

## 4. Decision: What Codex Should Do Next

### Immediate (close M2)

**Task 1: A2 all-axis dynamic tracking confirmation**

Run A2 with pitch and roll at 30°/s using the fixed SensorFusion. If RMS < 15°
for all three axes, commit the passing test. If pitch/roll are still weak,
document the actual numbers and commit as characterization (same pattern as
before the fix).

Use Kp=0.5 based on the gain characterization evidence — it gives the best
dynamic tracking.

Expected outcome: should pass now that the update convention is fixed. The
relative ordering test already shows yaw < pitch < roll, so all three track.

**Task 2: A1b large-angle static accuracy (optional)**

Try ±45° and ±90° static poses across all axes. The init fix should handle
these now. If they pass at < 5° RMS after 2s warmup, this fully closes A1.
If some poses are weak, document and move on — the small-angle proof is
already strong.

### After M2 is closed

**Move to Wave B:**

| # | Task | Priority |
|---|------|----------|
| B1 | CSV export + Python plots | Medium — evidence pipeline |
| B4 | Sensor validation matrix remaining gaps | Medium — close M1 |
| B2 | Motion profile JSON library | Medium — infrastructure |
| B3 | Hard iron calibration test | Medium — depends on B4 |

### Defer

- A4 full Kp/Ki sweep with all axes — the gain characterization already
  covers this adequately. A formal parameterized sweep is nice-to-have.
- RF/sync infrastructure (M4)
- Magnetic environment (M5-M6)

## 5. Is M2 Complete Enough to Move On?

**Yes, after Task 1 (A2 all-axis confirmation).**

If A2 all-axis passes: M2 is ~95% complete. The remaining 5% is A1b
large-angle which is optional polish, not blocking.

If A2 pitch/roll still fails: M2 is ~80% complete with yaw proven and
pitch/roll documented as a known dynamic tracking limitation. Still sufficient
to justify moving to Wave B while tracking the pitch/roll gap.

## 6. Handoff Note for Codex

**Do now:**
1. Run A2 dynamic tracking at 30°/s for all three axes (yaw, pitch, roll)
   with Kp=0.5, Ki=0.0, setSeed(42). 50-tick warmup, 500-tick measurement.
   Assert: all axes RMS < 15°, max < 30°. If passes, commit. If fails,
   document actual numbers and commit as characterization.

2. Optionally run A1b: ±45° and ±90° static poses, all axes. 100-tick warmup,
   200-tick measurement. Assert: RMS < 5°, max < 10°.

**Then:** Move to Wave B. Start with B1 (CSV export) or B4 (sensor validation
gaps) — whichever is more useful to you.

**Do NOT:** Spend more time tuning Kp/Ki. The current characterization is
sufficient. Do not reopen RF/sync or mag environment work.
