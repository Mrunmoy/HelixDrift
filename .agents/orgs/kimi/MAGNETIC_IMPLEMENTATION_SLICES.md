# Magnetic/Calibration Implementation Slices

**Document ID:** KIMI-MAG-SLICES-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (implementation-support)  
**Target:** Codex Sensor Validation Team  
**Source Spec:** `docs/MAGNETIC_CALIBRATION_RISK_SPEC.md`

---

## Executive Summary

The Magnetic/Calibration spec has been decomposed into 5 implementation slices. Slices 1-3 are **standalone sensor fidelity work** and can proceed in parallel with current Codex work. Slices 4-5 require integration with the harness and are higher risk.

**Critical Constraint:** Magnetic work must not destabilize existing 207 passing tests. All changes must be additive (new files/tests) or opt-in modifications.

---

## Slice 1: MagneticEnvironment Core (Foundation)

**Value/Cost Ratio:** 🔴 Highest (enables all disturbance testing)

### Scope
A spatially-varying magnetic field model with Earth field + dipole disturbances. Independent of existing sensor code—pure physics model.

### Files
- `simulators/magnetic/MagneticEnvironment.hpp`
- `simulators/magnetic/MagneticEnvironment.cpp`

### First Tests
```cpp
// test_magnetic_environment.cpp
TEST(MagneticEnvironment, EarthFieldOnlyUniformEverywhere) {
    MagneticEnvironment env;
    env.setEarthField({25.0f, 40.0f, 0.0f});  // Horizontal, vertical, declination
    
    Vec3 atOrigin = env.getFieldAt({0, 0, 0});
    Vec3 at1Meter = env.getFieldAt({1, 0, 0});
    
    // Same field everywhere with no disturbances
    EXPECT_NEAR(atOrigin.x, at1Meter.x, 0.01f);
    EXPECT_NEAR(atOrigin.y, at1Meter.y, 0.01f);
    EXPECT_NEAR(atOrigin.z, at1Meter.z, 0.01f);
    EXPECT_NEAR(atOrigin.magnitude(), 47.2f, 0.1f);  // sqrt(25² + 40²)
}

TEST(MagneticEnvironment, DipoleFieldDecaysWithDistance) {
    MagneticEnvironment env;
    env.setEarthField({25.0f, 40.0f, 0.0f});
    
    DipoleSource dipole;
    dipole.position = {0.5f, 0, 0};
    dipole.moment = {0, 0, 10.0f};  // 10 A·m² vertical
    env.addDipole(dipole);
    
    Vec3 nearDipole = env.getFieldAt({0.1f, 0, 0});
    Vec3 farDipole = env.getFieldAt({2.0f, 0, 0});
    
    // Near field should be much stronger disturbance
    float nearDisturbance = (nearDipole - env.getFieldAt({100, 0, 0})).magnitude();
    float farDisturbance = (farDipole - env.getFieldAt({100, 0, 0})).magnitude();
    
    EXPECT_GT(nearDisturbance, farDisturbance * 10);
}

TEST(MagneticEnvironment, PresetOfficeDeskHasReasonableDisturbance) {
    auto env = MagneticEnvironment::officeDesk();
    auto quality = env.getQualityAt({0, 0, 0});
    
    // Office desk should show some disturbance
    EXPECT_GT(quality.disturbanceRatio, 0.1f);
    EXPECT_LT(quality.disturbanceRatio, 2.0f);  // But not extreme
}
```

### Measurable Outputs
- [ ] Earth field uniform across 10m³ volume
- [ ] Dipole field follows inverse-cube law (within 5%)
- [ ] Field superposition linear (A + B = B + A within floating point error)
- [ ] Preset environments (cleanLab, officeDesk) return finite fields

### Explicitly Out of Scope
- ❌ Time-varying fields (future)
- ❌ Complex geometries (only dipole approximations)
- ❌ Integration with BMM350Simulator (Slice 2)
- ❌ Calibration algorithms

### Dependencies
- None (pure new code)

### Est. Effort
2-3 hours

### Anti-Scope-Creep Notes
- Dipole formula only: `B = (μ₀/4π) * (3(m·r̂)r̂ - m) / r³`
- No need for finite element or complex geometry
- Earth field is uniform vector, not WMM2015

---

## Slice 2: BMM350Simulator + MagneticEnvironment Integration

**Value/Cost Ratio:** 🔴 High (first sensor using spatial field)

### Scope
Optional modification to `Bmm350Simulator` to accept a `MagneticEnvironment*` and query field at sensor position. Fully backward-compatible (existing tests pass unchanged).

### Files
- Modify: `simulators/sensors/Bmm350Simulator.hpp` (add method)
- Modify: `simulators/sensors/Bmm350Simulator.cpp` (add method)

### Implementation Pattern
```cpp
// In Bmm350Simulator:
class Bmm350Simulator {
    // ... existing code ...
    
    // NEW: Optional environment for spatially-varying field
    void attachEnvironment(MagneticEnvironment* env, const Vec3& position);
    
private:
    MagneticEnvironment* env_ = nullptr;
    Vec3 envPosition_;  // Sensor position in environment coords
    
    // Modify computeMagField() to use env if attached
};
```

### First Tests
```cpp
// test_bmm350_environment.cpp
TEST(Bmm350WithEnvironment, ReadsFieldAtAttachedPosition) {
    MagneticEnvironment env;
    env.setEarthField({25.0f, 40.0f, 0.0f});
    
    Bmm350Simulator sensor;
    sensor.attachEnvironment(&env, {0, 0, 0});
    
    // Should read Earth field
    MagData data;
    ASSERT_TRUE(sensor.readMag(data));
    
    float mag = std::sqrt(data.x*data.x + data.y*data.y + data.z*data.z);
    EXPECT_NEAR(mag, 47.2f, 2.0f);  // Earth field magnitude
}

TEST(Bmm350WithEnvironment, DifferentPositionDifferentField) {
    // Add dipole, attach sensor at two positions, verify different readings
}

TEST(Bmm350Standalone, StillWorksWithoutEnvironment) {
    // Ensure backward compatibility - existing tests pass
    Bmm350Simulator sensor;
    // Don't call attachEnvironment
    // Should use internal DEFAULT_EARTH_FIELD as before
}
```

### Measurable Outputs
- [ ] All existing BMM350 tests still pass (backward compatibility)
- [ ] With environment attached, sensor reads environment field
- [ ] Without environment, sensor uses default static field (existing behavior)

### Explicitly Out of Scope
- ❌ Position updates (sensor is stationary relative to environment in v1)
- ❌ Multiple sensors sharing environment (Slice 5)
- ❌ Calibration state machine (Slice 3)

### Dependencies
- Requires Slice 1 (MagneticEnvironment)
- Touches `simulators/sensors/Bmm350Simulator.*` (Codex owned)

### Est. Effort
2-3 hours

### Risk Mitigation
- **Risk:** Modifying existing simulator could break 17 tests
- **Mitigation:** All changes are additive (new methods only), no changes to existing `readRegister` logic
- **Test:** Run `./build.py --host-only -t` before/after, expect 0 regressions

---

## Slice 3: Hard Iron Calibration (Standalone)

**Value/Cost Ratio:** 🔴 High (proves calibration concept)

### Scope
A calibration utility class that estimates hard iron offset from rotation data. Not integrated into sensor yet—pure algorithm.

### Files
- `simulators/magnetic/HardIronCalibrator.hpp`
- `simulators/magnetic/HardIronCalibrator.cpp`

### First Tests
```cpp
// test_hard_iron_calibration.cpp
TEST(HardIronCalibrator, EstimatesOffsetFromSphereFit) {
    // Simulate readings with known offset
    Vec3 trueOffset{10.0f, -5.0f, 3.0f};
    
    HardIronCalibrator cal;
    cal.startCalibration();
    
    // Feed samples from rotated orientations (ellipsoid)
    for (int i = 0; i < 100; i++) {
        float angle = i * 2 * M_PI / 100;
        Vec3 field = rotateField({25, 0, -40}, angle) + trueOffset;
        cal.addSample(field);
    }
    
    Vec3 estimated = cal.getOffset();
    
    EXPECT_NEAR(estimated.x, trueOffset.x, 1.0f);
    EXPECT_NEAR(estimated.y, trueOffset.y, 1.0f);
    EXPECT_NEAR(estimated.z, trueOffset.z, 1.0f);
}

TEST(HardIronCalibrator, CoverageDetection) {
    // Too few samples → confidence < threshold
    // Good distribution → confidence > threshold
}
```

### Measurable Outputs
- [ ] Offset estimation within ±2 µT for 10 µT injected offset
- [ ] Coverage confidence metric (0-1 based on sample distribution)
- [ ] Fails gracefully with insufficient data (returns {0,0,0} or error)

### Explicitly Out of Scope
- ❌ Soft iron (ellipsoid fitting, matrix estimation)
- ❌ Temperature compensation
- ❌ Real-time updates (batch algorithm only)
- ❌ Integration with BMM350Simulator (Slice 4)

### Dependencies
- None (pure algorithm on Vec3 data)

### Est. Effort
3-4 hours

### Algorithm Notes
- Simple sphere fitting: minimize `sum((|B_i - offset| - R)²)`
- Use gradient descent or algebraic solution (not production-grade, good enough for sim)
- Confidence = fraction of sphere surface sampled (angular histogram)

---

## Slice 4: CalibratedMagSensor Wrapper (Integration)

**Value/Cost Ratio:** 🟡 Medium (high integration risk)

### Scope
A wrapper that combines BMM350Simulator + MagneticEnvironment + HardIronCalibrator. This is the "calibrated sensor" interface for tests.

### Files
- `simulators/magnetic/CalibratedMagSensor.hpp`
- `simulators/magnetic/CalibratedMagSensor.cpp`

### First Tests
```cpp
// test_calibrated_mag_sensor.cpp
TEST(CalibratedMagSensor, CalibrationRemovesHardIron) {
    MagneticEnvironment env = MagneticEnvironment::cleanLab();
    Bmm350Simulator bmm;
    bmm.attachEnvironment(&env, {0, 0, 0});
    
    CalibratedMagSensor sensor(bmm, env, {0, 0, 0});
    
    // Inject known error
    sensor.injectHardIron({10, -5, 3});
    
    // Calibrate
    sensor.startCalibration();
    // ... rotate through orientations ...
    
    // Verify calibrated field matches environment
    Vec3 calibrated;
    sensor.readCalibrated(calibrated);
    
    Vec3 expected = env.getFieldAt({0, 0, 0});
    EXPECT_NEAR(calibrated.x, expected.x, 2.0f);
    EXPECT_NEAR(calibrated.y, expected.y, 2.0f);
    EXPECT_NEAR(calibrated.z, expected.z, 2.0f);
}
```

### Measurable Outputs
- [ ] Calibration state machine progresses correctly
- [ ] Hard iron compensation improves field accuracy
- [ ] Disturbance indicator rises when near dipole sources

### Explicitly Out of Scope
- ❌ Soft iron matrix estimation (v2)
- ❌ Auto-recalibration during operation
- ❌ AHRS integration (Slice 5)

### Dependencies
- Requires Slices 1-3
- High integration surface area

### Est. Effort
4-5 hours

### Risk Warning
This slice touches the most components. Consider deferring until Slices 1-3 are solid and reviewed.

---

## Slice 5: AHRS Robustness Testing (Advanced)

**Value/Cost Ratio:** 🟢 Lower (integration with fusion)

### Scope
Tests that verify AHRS maintains heading during temporary magnetic disturbances. Uses full `VirtualSensorAssembly` with disturbed magnetic field.

### Files
- `simulators/tests/test_ahrs_mag_robustness.cpp`

### First Tests
```cpp
TEST(AHRSRobustness, HeadingRecoversAfterDisturbance) {
    // Setup assembly with magnetic environment
    // Record heading in clean field
    // Add disturbance (laptop nearby)
    // Verify heading error bounded during disturbance
    // Remove disturbance
    // Verify heading recovers within 15°
}
```

### Measurable Outputs
- [ ] Heading error < 15° during moderate disturbance
- [ ] Recovery to < 5° error within 3 seconds after removal

### Explicitly Out of Scope
- ❌ Adaptive magnetometer weighting
- ❌ Disturbance rejection algorithms
- ❌ Multi-node scenarios

### Dependencies
- Requires Slice 4
- Requires stable VirtualSensorAssembly

### Est. Effort
3-4 hours
- **RECOMMENDATION:** Defer until after RF work is stable

---

## Ranking Summary

| Slice | Value/Cost | Risk | Can Start Now? | Effort |
|-------|------------|------|----------------|--------|
| 1. MagneticEnvironment | 🔴 Highest | Low | ✅ Yes (new files only) | 2-3h |
| 2. BMM350 Integration | 🔴 High | Medium | ✅ Yes (additive changes) | 2-3h |
| 3. Hard Iron Calibrator | 🔴 High | Low | ✅ Yes (pure algorithm) | 3-4h |
| 4. CalibratedMagSensor | 🟡 Medium | High | After 1-3 | 4-5h |
| 5. AHRS Robustness | 🟢 Lower | High | After 1-4, defer | 3-4h |

**Recommended First Codex Slice:** Slice 1 (MagneticEnvironment Core)

**Recommended Order:** 1 → 3 → 2 → (defer 4-5 until RF work stable)

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Slice 2 breaks existing BMM350 tests | Medium | High | Additive changes only, full regression test |
| Slice 4 coupling causes instability | Medium | Medium | Defer to dedicated integration session |
| Dipole physics incorrect | Low | Medium | Validate with known formulas in unit tests |
| Hard iron algorithm diverges | Low | Low | Clamp iterations, provide fallback |

---

## No-Overlap Confirmation

Per `.agents/OWNERSHIP_MATRIX.md`:
- ✅ `simulators/magnetic/` is new directory → Codex owned
- ✅ Slice 2 touches `Bmm350Simulator` → Codex owned
- ✅ Kimi writes spec only, does not implement
- ✅ Claude handles architecture sequencing

## Anti-Scope-Creep Checklist

For each slice, if Codex encounters:
- [ ] Need to modify `MahonyAHRS` → STOP, ask Claude
- [ ] Need to modify `MocapNodePipeline` → STOP, ask Claude
- [ ] Need to add soft iron (3x3 matrix) → DEFER to v2
- [ ] Need temperature model → DEFER, document in spec
- [ ] Need moving sensor (position updates) → DEFER to kinematics work
