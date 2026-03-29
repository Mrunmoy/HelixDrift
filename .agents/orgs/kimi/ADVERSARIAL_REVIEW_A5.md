# Adversarial Review: A5 Gyro Bias Rejection Test (f9295c6)

**Reviewer:** Kimi  
**Date:** 2026-03-29  
**Test:** `PoseMahonyTuningTest.GyroZBiasWithoutIntegralFeedback` / `IntegralFeedbackReducesHeadingErrorFromGyroZBias`  
**Commit:** f9295c6

---

## Executive Summary

The A5 test demonstrates that Mahony integral feedback (Ki) reduces drift from gyro bias under **idealized conditions**. The test is **worth keeping** as a baseline proof, but three key assumptions make it easier than real-world operation and could give a false sense of robustness.

---

## Top 3 Risks in Current A5 Test

### Risk 1: Z-Axis Bias Only (Easiest Case)

**What the test does:** Injects 0.01 rad/s bias **only on Z-axis** (yaw rate).

**Why this is easier than real life:**
- Z-axis bias produces pure yaw rotation — the magnetometer can observe this directly as heading change
- X/Y bias produce tilt errors that couple into heading through the non-linear fusion
- Real MEMS gyros often have **similar bias magnitude on all axes**, not just Z

**Real-world impact:** The test proves Ki helps with the easiest bias case. X/Y bias rejection could be weaker.

### Risk 2: Clean Magnetic Field (Zero Disturbance)

**What the test does:** Uses default `VirtualSensorAssembly` with **undisturbed Earth field**.

**Why this is easier than real life:**
- Mahony integral feedback assumes mag North is a reliable reference
- In disturbed fields (laptop, furniture), the "reference" itself is biased
- Ki will **integrate mag disturbance into the bias estimate**, causing false correction

**Real-world impact:** The test proves Ki works when mag is perfect. Near electronics, Ki can actually hurt by "learning" the disturbance as bias.

### Risk 3: Fixed 20ms dt, Zero Jitter

**What the test does:** Runs with `stepUs = 20000` (exactly 50 Hz), zero timing jitter.

**Why this is easier than real life:**
- Mahony filter's integral term accumulates `error * dt`
- Variable dt or missed samples cause incorrect integral accumulation
- Real systems have scheduling jitter, I2C delays, radio latency

**Real-world impact:** The test assumes perfect timing. Real-world jitter reduces Ki effectiveness and can introduce instability.

---

## Does the Test Prove Integral Feedback Meaningfully?

| Condition | Test Coverage | Real-World Match |
|-----------|--------------|------------------|
| Z-axis bias only | ✅ Tested | ⚠️ Easiest case |
| X/Y axis bias | ❌ Not tested | ❌ Harder case |
| Clean mag field | ✅ Tested | ⚠️ Idealized |
| Disturbed mag field | ❌ Not tested | ❌ Ki can hurt |
| Fixed dt | ✅ Tested | ⚠️ Idealized |
| Variable timing | ❌ Not tested | ❌ Reduces effectiveness |

**Verdict:** The test proves Ki **can** work under ideal conditions. It does **not** prove Ki **will** work in real conditions.

---

## Is the Current Result Worth Keeping?

**Yes, with caveats:**

✅ **Keep the test because:**
- It establishes a baseline: Ki reduces drift from bias in principle
- It validates the Mahony implementation has working integral feedback
- It provides a regression check (if Ki breaks, test fails)

⚠️ **Document the limitations:**
- Add comment: "Idealized test — Z-bias only, clean mag field, fixed dt"
- Note: "Real-world performance may vary with X/Y bias, mag disturbances, timing jitter"

---

## One Minimal Follow-Up Test (M2-Compatible)

**Test Name:** `GyroXYBiasWithIntegralFeedbackShowsTiltError`

**Why this matters:** X/Y bias creates tilt error, which couples into heading through the fusion non-linearity. This is harder to correct than pure yaw.

**Implementation (add to same file):**
```cpp
TEST(PoseMahonyTuningTest, GyroXYBiasHarderThanZBias) {
    // X-bias case (tilt-inducing)
    VirtualMocapNodeHarness::Config configX;
    configX.pipeline.mahonyKp = 1.0f;
    configX.pipeline.mahonyKi = 0.1f;  // Best Ki from A5
    
    VirtualMocapNodeHarness harnessX(configX);
    harnessX.setSeed(42);
    harnessX.initAll();
    harnessX.resetAndSync();
    harnessX.assembly().imuSim().setGyroBias({0.01f, 0.0f, 0.0f});  // X bias
    
    auto resultX = harnessX.runWithWarmup(50, 1500, 20000);
    
    // Z-bias case (from A5, for comparison)
    VirtualMocapNodeHarness::Config configZ;
    configZ.pipeline.mahonyKp = 1.0f;
    configZ.pipeline.mahonyKi = 0.1f;
    
    VirtualMocapNodeHarness harnessZ(configZ);
    harnessZ.setSeed(42);
    harnessZ.initAll();
    harnessZ.resetAndSync();
    harnessZ.assembly().imuSim().setGyroBias({0.0f, 0.0f, 0.01f});  // Z bias
    
    auto resultZ = harnessZ.runWithWarmup(50, 1500, 20000);
    
    // X-bias should be harder to correct (larger final error)
    EXPECT_GT(resultX.finalErrorDeg, resultZ.finalErrorDeg);
    
    // But Ki should still help vs no Ki
    EXPECT_LT(resultX.finalErrorDeg, 5.0f);  // Better than unbounded drift
}
```

**Effort:** ~15 minutes to add  
**Value:** Exposes the Z-bias limitation without new infrastructure

---

## What Should Wait Until Deferred RF/Magnetic Work

| Feature | Why Deferred | When Appropriate |
|---------|--------------|------------------|
| **Magnetic disturbance + Ki interaction** | Requires `MagneticEnvironment` (Mag Slice 1) | M5 calibration work |
| **Timing jitter effects** | Requires `VirtualRFMedium` variable latency (RF Slice 1) | M4 sync work |
| **Multi-axis combined bias** | Can be added now, but not critical | Wave B if time permits |
| **Temperature-dependent bias** | Requires temp model (not in sim) | Post-M2 |

**Do not derail M2 for these.** The current A5 test is sufficient for M2's "prove algorithms work under controlled conditions" goal.

---

## Recommendation Summary

1. **Keep A5 test** — it proves Ki works in principle
2. **Add XY-bias comparison test** — 15 min, exposes Z-axis limitation
3. **Document assumptions** in test comments — prevents false confidence
4. **Defer mag disturbance testing** — requires MagneticEnvironment (M5)
5. **Defer timing jitter testing** — requires VirtualRFMedium (M4)

---

**Status:** ✅ Review complete — A5 test acceptable for M2 with minor addition
