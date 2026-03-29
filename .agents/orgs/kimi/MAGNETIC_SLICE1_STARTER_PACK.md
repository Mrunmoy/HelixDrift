# Magnetic Slice 1 Starter Pack: MagneticEnvironment Core

**Document ID:** KIMI-MAG-S1-STARTER-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (implementation-support)  
**Target:** Codex Sensor Validation Team (deferred until M2 closes)  
**Effort:** 2-3 hours  
**Risk:** Low (new files only, zero M2 impact)

---

## Overview

This starter pack provides everything Codex needs to implement MagneticEnvironment Core when M2 closes. It is designed to be:
- **100% additive** (new directory `simulators/magnetic/`, no existing file changes)
- **Zero M2 risk** (does not touch any files Codex is currently working on)
- **Self-contained** (pure physics model, no sensor dependencies)

---

## Exact File List

### New Files (4 total)
```
simulators/magnetic/
├── MagneticEnvironment.hpp      # Interface definition
├── MagneticEnvironment.cpp      # Implementation
└── tests/
    └── test_magnetic_environment.cpp  # First 3 tests (TDD)
```

### CMakeLists.txt Addition
Add to `CMakeLists.txt` in the `helix_simulators` library section:
```cmake
# simulators/magnetic/CMakeLists.txt or add to main CMakeLists.txt
simulators/magnetic/MagneticEnvironment.cpp
```

And to `helix_integration_tests`:
```cmake
simulators/tests/test_magnetic_environment.cpp
```

---

## Interface Sketch

### MagneticEnvironment.hpp
```cpp
#pragma once

#include "Vec3.hpp"
#include <cstdint>
#include <vector>
#include <random>

namespace sim {

struct DipoleSource {
    sf::Vec3 position;      // Position in world coordinates (meters)
    sf::Vec3 moment;        // Magnetic moment (A·m²)
    float decayRadius = 5.0f;  // Distance where field becomes negligible (optimization)
    
    // Constructor for convenience
    DipoleSource(sf::Vec3 pos, sf::Vec3 mom) 
        : position(pos), moment(mom) {}
};

struct EarthFieldModel {
    float horizontal;       // µT (north component)
    float vertical;         // µT (positive = down)
    float declination;      // Degrees from true north (unused in v1, for future)
    
    // Default constructor for typical values
    EarthFieldModel(float h = 25.0f, float v = 40.0f, float d = 0.0f)
        : horizontal(h), vertical(v), declination(d) {}
    
    // Convert to 3D vector in world coordinates (x=north, y=east, z=down)
    sf::Vec3 toVector() const {
        return {horizontal, 0.0f, vertical};  // Simplified: no declination in v1
    }
};

class MagneticEnvironment {
public:
    MagneticEnvironment();
    
    // Earth field configuration
    void setEarthField(const EarthFieldModel& field);
    EarthFieldModel getEarthField() const { return earthField_; }
    
    // Disturbance sources
    void addDipole(const DipoleSource& dipole);
    void clearDipoles();
    size_t getDipoleCount() const { return dipoles_.size(); }
    
    // Field query at a position (meters)
    sf::Vec3 getFieldAt(const sf::Vec3& position) const;
    
    // Quality metrics at a position
    struct FieldQuality {
        float totalFieldUt;          // |B|
        float earthFieldUt;          // |B_earth|
        float disturbanceUt;         // |B_disturbance|
        float disturbanceRatio;      // |B_disturbance| / |B_earth|
        bool isDisturbed;            // disturbanceRatio > threshold (0.2)
    };
    FieldQuality getQualityAt(const sf::Vec3& position) const;
    
    // Preset environments
    static MagneticEnvironment cleanLab();      // Earth field only
    static MagneticEnvironment officeDesk();    // Earth + laptop dipole
    static MagneticEnvironment worstCase();     // Strong interference

private:
    EarthFieldModel earthField_;
    std::vector<DipoleSource> dipoles_;
    
    // Dipole field calculation: B = (μ₀/4π) * (3(m·r̂)r̂ - m) / r³
    // Returns field in µT
    sf::Vec3 computeDipoleField(const DipoleSource& dipole, 
                                 const sf::Vec3& position) const;
    
    // μ₀/4π ≈ 0.1 µT·m/A for unit conversion
    static constexpr float kMu0Over4Pi = 0.1f;
};

} // namespace sim
```

### MagneticEnvironment.cpp Key Implementation Notes

```cpp
// Constructor - default Earth field
MagneticEnvironment::MagneticEnvironment()
    : earthField_(25.0f, 40.0f, 0.0f)
{}

// Main query - Earth field + sum of all dipoles
sf::Vec3 MagneticEnvironment::getFieldAt(const sf::Vec3& position) const {
    sf::Vec3 totalField = earthField_.toVector();
    
    for (const auto& dipole : dipoles_) {
        float dist = (position - dipole.position).magnitude();
        if (dist > dipole.decayRadius) continue;  // Optimization
        totalField = totalField + computeDipoleField(dipole, position);
    }
    
    return totalField;
}

// Dipole formula: B = (μ₀/4π) * (3(m·r̂)r̂ - m) / r³
sf::Vec3 MagneticEnvironment::computeDipoleField(
    const DipoleSource& dipole, const sf::Vec3& position) const {
    
    sf::Vec3 r = position - dipole.position;
    float rMag = r.magnitude();
    if (rMag < 0.001f) return {1000.0f, 1000.0f, 1000.0f};  // Singularity guard
    
    sf::Vec3 rHat = r / rMag;
    float rCubed = rMag * rMag * rMag;
    
    float mDotRhat = dipole.moment.x * rHat.x + 
                     dipole.moment.y * rHat.y + 
                     dipole.moment.z * rHat.z;
    
    sf::Vec3 term = {
        3.0f * mDotRhat * rHat.x - dipole.moment.x,
        3.0f * mDotRhat * rHat.y - dipole.moment.y,
        3.0f * mDotRhat * rHat.z - dipole.moment.z
    };
    
    float scale = kMu0Over4Pi / rCubed;
    return {term.x * scale, term.y * scale, term.z * scale};
}
```

---

## Test File Skeleton

### test_magnetic_environment.cpp
```cpp
#include <gtest/gtest.h>
#include "MagneticEnvironment.hpp"

using namespace sim;
using namespace sf;

// Test 1: Earth field is uniform everywhere
TEST(MagneticEnvironment, EarthFieldOnlyUniformEverywhere) {
    MagneticEnvironment env;
    env.setEarthField({25.0f, 40.0f, 0.0f});
    
    Vec3 atOrigin = env.getFieldAt({0, 0, 0});
    Vec3 at1MeterX = env.getFieldAt({1, 0, 0});
    Vec3 at1MeterY = env.getFieldAt({0, 1, 0});
    Vec3 at10Meters = env.getFieldAt({10, 10, 10});
    
    // All should be identical (uniform field)
    EXPECT_NEAR(atOrigin.x, at1MeterX.x, 0.01f);
    EXPECT_NEAR(atOrigin.y, at1MeterX.y, 0.01f);
    EXPECT_NEAR(atOrigin.z, at1MeterX.z, 0.01f);
    
    EXPECT_NEAR(atOrigin.x, at10Meters.x, 0.01f);
    EXPECT_NEAR(atOrigin.y, at10Meters.y, 0.01f);
    EXPECT_NEAR(atOrigin.z, at10Meters.z, 0.01f);
    
    // Magnitude: sqrt(25^2 + 40^2) ≈ 47.17 µT
    EXPECT_NEAR(atOrigin.magnitude(), 47.17f, 0.1f);
}

// Test 2: Dipole field decays with distance (inverse cube law)
TEST(MagneticEnvironment, DipoleFieldDecaysWithDistance) {
    MagneticEnvironment env;
    env.setEarthField({0, 0, 0});  // Zero Earth field for clarity
    
    // Add dipole at origin, moment along Z
    env.addDipole(DipoleSource({0, 0, 0}, {0, 0, 10.0f}));
    
    // Measure at 0.1m and 0.2m along X axis
    Vec3 at01m = env.getFieldAt({0.1f, 0, 0});
    Vec3 at02m = env.getFieldAt({0.2f, 0, 0});
    
    float mag01 = at01m.magnitude();
    float mag02 = at02m.magnitude();
    
    // Inverse cube law: doubling distance -> 1/8 field
    EXPECT_NEAR(mag02, mag01 / 8.0f, mag01 / 16.0f);
    EXPECT_LT(mag02, mag01);
}

// Test 3: Superposition principle (fields add linearly)
TEST(MagneticEnvironment, FieldSuperpositionLinear) {
    MagneticEnvironment env;
    env.setEarthField({0, 0, 0});
    
    // Dipole A: at (1,0,0), moment X
    env.addDipole(DipoleSource({1, 0, 0}, {10, 0, 0}));
    
    // Dipole B: at (0,1,0), moment Y
    env.addDipole(DipoleSource({0, 1, 0}, {0, 10, 0}));
    
    Vec3 combined = env.getFieldAt({0, 0, 0});
    
    // Field from A only
    MagneticEnvironment envA;
    envA.addDipole(DipoleSource({1, 0, 0}, {10, 0, 0}));
    Vec3 fieldA = envA.getFieldAt({0, 0, 0});
    
    // Field from B only
    MagneticEnvironment envB;
    envB.addDipole(DipoleSource({0, 1, 0}, {0, 10, 0}));
    Vec3 fieldB = envB.getFieldAt({0, 0, 0});
    
    // Combined should equal A + B
    EXPECT_NEAR(combined.x, fieldA.x + fieldB.x, 0.01f);
    EXPECT_NEAR(combined.y, fieldA.y + fieldB.y, 0.01f);
    EXPECT_NEAR(combined.z, fieldA.z + fieldB.z, 0.01f);
}

// Bonus: Preset environments produce finite, reasonable fields
TEST(MagneticEnvironment, PresetsProduceFiniteFields) {
    auto clean = MagneticEnvironment::cleanLab();
    auto office = MagneticEnvironment::officeDesk();
    auto worst = MagneticEnvironment::worstCase();
    
    Vec3 atOrigin;
    
    atOrigin = clean.getFieldAt({0, 0, 0});
    EXPECT_GT(atOrigin.magnitude(), 10.0f);
    EXPECT_LT(atOrigin.magnitude(), 100.0f);
    
    atOrigin = office.getFieldAt({0, 0, 0});
    EXPECT_GT(atOrigin.magnitude(), 10.0f);
    EXPECT_LT(atOrigin.magnitude(), 100.0f);
    
    auto quality = office.getQualityAt({0, 0, 0});
    EXPECT_GT(quality.disturbanceRatio, 0.05f);
    
    atOrigin = worst.getFieldAt({0, 0, 0});
    EXPECT_GT(atOrigin.magnitude(), 10.0f);
}
```

---

## First 3 Tests in Execution Order

### Test 1: EarthFieldOnlyUniformEverywhere (15 min)
**Purpose:** Prove Earth field is uniform and correctly configured  
**Setup:** Set Earth field, query at multiple positions  
**Assert:** All positions return identical vector, magnitude correct  
**Why first:** Validates base field model without dipole complexity

### Test 2: DipoleFieldDecaysWithDistance (20 min)
**Purpose:** Prove dipole field follows inverse-cube law  
**Setup:** Single dipole, measure at two distances  
**Assert:** Field decays by ~1/8 when distance doubles  
**Why second:** Validates physics model before adding complexity

### Test 3: FieldSuperpositionLinear (15 min)
**Purpose:** Prove multiple dipoles add linearly  
**Setup:** Two dipoles, compare combined vs individual sum  
**Assert:** Combined = A + B  
**Why third:** Validates multi-source capability

---

## What NOT to Implement Yet

### Explicitly Out of Scope for Slice 1
- ❌ **Time-varying fields** (future: animating dipoles)
- ❌ **Complex geometries** (finite element models, shapes)
- ❌ **Integration with BMM350Simulator** (Slice 2: adds attachEnvironment())
- ❌ **Calibration algorithms** (Slice 3: HardIronCalibrator)
- ❌ **World Magnetic Model** (WMM2015 - too complex for sim)
- ❌ **Temperature effects** on field (future)

### Do Not Touch These Files
- ❌ `Bmm350Simulator.hpp/cpp` (M2 active)
- ❌ `VirtualMocapNodeHarness.hpp/cpp` (Claude owns)
- ❌ `VirtualSensorAssembly.hpp/cpp` (Codex currently using for M2)
- ❌ Any existing sensor test files (add new file only)

---

## Anti-Scope-Creep Checklist

If during implementation you find yourself wanting to:
- [ ] Add time-varying fields → STOP, future feature
- [ ] Add WMM2015 model → STOP, unnecessary complexity
- [ ] Add soft iron (ferromagnetic distortion) → STOP, v2 feature
- [ ] Modify BMM350Simulator → STOP, that's Slice 2
- [ ] Add sensor integration → STOP, defer until Slice 2
- [ ] Make it template/generic → STOP, YAGNI for now

---

## Readiness Assessment: Can Codex Start This Later?

| Criterion | Status |
|-----------|--------|
| Additive only (no file modifications) | ✅ Yes |
| New directory (no conflicts with M2) | ✅ Yes |
| No dependencies on sensor code | ✅ Yes |
| Self-contained tests | ✅ Yes |
| Low risk of breaking 220+ tests | ✅ Yes |
| Clear completion criteria | ✅ Yes |
| Physics validated in tests | ✅ Yes |

**Verdict:** This slice is ready for deferred implementation. Codex can pick it up immediately after M2 closes without any M2 destabilization risk.

---

## Dipole Physics Reference

### Formula Used
```
B = (μ₀/4π) * (3(m·r̂)r̂ - m) / r³
```

Where:
- B = magnetic field vector (µT)
- μ₀/4π = 0.1 µT·m/A (approximate for unit conversion)
- m = magnetic moment (A·m²)
- r̂ = unit vector from dipole to observation point
- r = distance from dipole (m)

### Units
- Position: meters
- Moment: A·m² (ampere-meter squared)
- Field output: µT (microtesla)

### Typical Values
- Earth field: 25-65 µT (location dependent)
- Laptop at 0.5m: ~5-15 µT disturbance
- Strong magnet at 0.1m: ~100+ µT disturbance

---

## Post-Implementation Next Steps

After this slice is implemented and tests pass:
1. Update `simulators/docs/DEV_JOURNAL.md`
2. Request Kimi review
3. Request Claude review
4. Then proceed to **Magnetic Slice 3: HardIronCalibrator** (can parallelize with RF work)
5. Then **Magnetic Slice 2: BMM350 Integration** (touches existing code, be careful)

---

**Status:** ✅ Starter pack complete - ready for deferred implementation
