# Magnetic/Calibration Risk Simulation Specification

**Document ID:** SIM-MAGRISK-001  
**Version:** 1.0  
**Date:** 2026-03-29  
**Author:** Kimi (research/review)  
**Target:** Codex implementation team (Milestone 5-6)  
**Status:** Implementation-Ready

---

## 1. Purpose

Define simulation infrastructure for validating magnetometer performance under realistic magnetic disturbances and calibration states. This enables pre-hardware testing of:

- Hard/soft iron compensation algorithms
- Magnetic disturbance detection and rejection
- Calibration quality estimation
- AHRS robustness to field variations

---

## 2. Problem Statement

The current BMM350 simulator assumes:
- Constant Earth magnetic field (25 µT horizontal, 40 µT vertical)
- Perfect calibration OTP data
- No external magnetic disturbances

**Real-world conditions that break these assumptions:**
1. **Body effects**: Ferromagnetic materials (batteries, motors) create local fields
2. **Furniture**: Metal desks, filing cabinets distort field
3. **Electronics**: Laptops, monitors, power supplies generate fields
4. **Building structure**: Steel framing, rebar modify local field
5. **Temporal changes**: Moving through space changes observed field

---

## 3. Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                     MagneticEnvironment                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │  Earth Field │  │ Disturbances │  │   Dipoles    │              │
│  │  (reference) │  │  (furniture) │  │  (body-worn) │              │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘              │
│         │                 │                  │                      │
│         └─────────────────┴──────────────────┘                      │
│                           │                                         │
│                    ┌──────┴──────┐                                  │
│                    │ Field Model │                                  │
│                    │  B(r, t)    │                                  │
│                    └──────┬──────┘                                  │
│                           │                                         │
└───────────────────────────┼─────────────────────────────────────────┘
                            │
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
        ┌─────────┐   ┌─────────┐   ┌─────────┐
        │  Node 1 │   │  Node 2 │   │  Node N │
        │(sensor) │   │(sensor) │   │(sensor) │
        └─────────┘   └─────────┘   └─────────┘
```

---

## 4. Component Specifications

### 4.1 MagneticEnvironment

**Location:** `simulators/magnetic/MagneticEnvironment.hpp`  
**Responsibility:** Models the spatially-varying magnetic field

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
    float decayRadius;      // Distance where field becomes negligible
    bool isTimeVarying;     // Whether moment changes over time
    float variationFreqHz;  // For time-varying sources
};

struct EarthFieldModel {
    float horizontal;       // µT
    float vertical;         // µT (positive = down)
    float declination;      // Degrees from true north
    float inclination;      // Degrees from horizontal
    
    static EarthFieldModel defaultForRegion(float latitude, float longitude);
};

class MagneticEnvironment {
public:
    MagneticEnvironment();
    
    // Earth field configuration
    void setEarthField(const EarthFieldModel& field);
    
    // Disturbance sources
    void addDipole(const DipoleSource& dipole);
    void removeDipole(size_t index);
    void clearDipoles();
    
    // Temporal variation
    void setTimeVarying(bool enable, float timeConstant);
    void advanceTime(float deltaT);
    
    // Field query
    sf::Vec3 getFieldAt(const sf::Vec3& position) const;
    
    // Quality metrics at a position
    struct FieldQuality {
        float totalFieldUt;          // |B|
        float earthAlignment;        // cos(angle between B and Earth)
        float disturbanceRatio;      // |B_disturbance| / |B_earth|
        bool isDisturbed;            // disturbanceRatio > threshold
    };
    FieldQuality getQualityAt(const sf::Vec3& position) const;
    
    // Preset environments
    static MagneticEnvironment cleanLab();           // Ideal conditions
    static MagneticEnvironment officeDesk();         // Laptop, monitor nearby
    static MagneticEnvironment wearableMotion();     // Body-worn, moving
    static MagneticEnvironment worstCase();          // Severe interference

private:
    EarthFieldModel earthField_;
    std::vector<DipoleSource> dipoles_;
    float currentTime_ = 0.0f;
    bool timeVarying_ = false;
    mutable std::mt19937 rng_;
    
    sf::Vec3 computeDipoleField(const DipoleSource& dipole, 
                                 const sf::Vec3& position) const;
};

} // namespace sim
```

### 4.2 CalibratedMagSensor

**Location:** `simulators/magnetic/CalibratedMagSensor.hpp`  
**Responsibility:** Wraps BMM350 simulator with calibration state machine

```cpp
#pragma once

#include "Bmm350Simulator.hpp"
#include "MagneticEnvironment.hpp"
#include <array>

namespace sim {

// Calibration state progression
enum class CalibrationState {
    UNCALIBRATED,           // Raw sensor output
    HARD_IRON_ESTIMATED,    // Offset removed
    SOFT_IRON_PARTIAL,      // Approximate scale
    SOFT_IRON_FULL,         // Full 3x3 matrix
    FIELD_VALIDATED         // Verified against known field
};

struct HardIronCal {
    sf::Vec3 offset{0, 0, 0};    // Estimated hard iron (µT)
    uint32_t sampleCount = 0;
    float confidence = 0.0f;      // 0-1 based on coverage
};

struct SoftIronCal {
    std::array<float, 9> matrix;  // 3x3 row-major, should be ~identity
    float conditionNumber;        // Matrix condition (ill-conditioned if > 10)
};

class CalibratedMagSensor {
public:
    CalibratedMagSensor(Bmm350Simulator& sensor, 
                        MagneticEnvironment& environment,
                        const sf::Vec3& mountPosition);
    
    // Calibration progression
    void startCalibration();
    void addCalibrationSample(const sf::Quaternion& orientation);
    CalibrationState getCalibrationState() const;
    
    // Apply/revoke calibration
    void applyCalibration();
    void setCalibrationState(CalibrationState state);
    
    // Error injection (for testing calibration)
    void injectHardIron(const sf::Vec3& offset);
    void injectSoftIron(const std::array<float, 9>& matrix);
    void setCalibrationQuality(float quality);  // 0.0 = poor, 1.0 = perfect
    
    // Quality metrics
    struct CalibrationQuality {
        float fieldMagnitudeError;    // |B_measured| vs |B_earth|
        float headingErrorDeg;        // From known orientation
        float stabilityScore;         // Variance over time
        CalibrationState state;
    };
    CalibrationQuality evaluateCalibration() const;
    
    // Read calibrated data
    bool readCalibratedMag(sf::Vec3& outFieldUt);
    float getDisturbanceIndicator() const;  // 0 = clean, 1 = disturbed

private:
    BMM350Simulator& sensor_;
    MagneticEnvironment& env_;
    sf::Vec3 mountPosition_;
    
    CalibrationState state_ = CalibrationState::UNCALIBRATED;
    HardIronCal hardIron_;
    SoftIronCal softIron_;
    float calibrationQuality_ = 1.0f;
    
    std::vector<sf::Vec3> calibrationSamples_;
    
    void estimateHardIron();
    void estimateSoftIron();
};

} // namespace sim
```

### 4.3 DisturbanceScenario

**Location:** `simulators/magnetic/DisturbanceScenario.hpp`  
**Responsibility:** Pre-defined test scenarios for reproducible validation

```cpp
#pragma once

#include "MagneticEnvironment.hpp"
#include "VirtualGimbal.hpp"
#include <string>
#include <vector>

namespace sim {

// Pre-defined disturbance scenarios
struct DisturbanceScenario {
    std::string name;
    std::string description;
    
    // Environment setup
    MagneticEnvironment env;
    
    // Motion profile (gimbal commands)
    struct MotionSegment {
        float durationSec;
        sf::Vec3 rotationAxis;
        float rotationRateDps;
        sf::Vec3 translationVelocity;  // For position-dependent fields
    };
    std::vector<MotionSegment> motion;
    
    // Expected outcomes (for test validation)
    struct ExpectedOutcome {
        float maxHeadingErrorDeg;
        float maxTiltErrorDeg;
        bool shouldDetectDisturbance;
        bool shouldRecover;
        float recoveryTimeSec;
    } expected;
    
    // Factory methods for common scenarios
    static DisturbanceScenario stationaryClean();
    static DisturbanceScenario rotateAndReturn();
    static DisturbanceScenario nearbyFerromagnetic();
    static DisturbanceScenario movingPastFurniture();
    static DisturbanceScenario wearableWalking();
    static DisturbanceScenario extremeInterference();
};

} // namespace sim
```

---

## 5. Test Scenarios

### 5.1 Test: Hard Iron Compensation

**File:** `simulators/tests/test_mag_hard_iron.cpp`

```cpp
TEST(MagCalibration, HardIronCompensationRemovesOffset) {
    MagneticEnvironment env = MagneticEnvironment::cleanLab();
    BMM350Simulator sensor;
    CalibratedMagSensor calSensor(sensor, env, /*position*/ {0, 0, 0});
    
    // Inject known hard iron
    sf::Vec3 injectedOffset{10.0f, -5.0f, 3.0f};  // µT
    calSensor.injectHardIron(injectedOffset);
    
    // Perform calibration rotation (figure-8 pattern)
    calSensor.startCalibration();
    for (int i = 0; i < 360; ++i) {
        float angle = i * M_PI / 180.0f;
        sf::Quaternion q = sf::Quaternion::fromAxisAngle(
            std::cos(angle), std::sin(angle), 0.2f, 10.0f);
        calSensor.addCalibrationSample(q);
    }
    
    // Verify calibration state advanced
    EXPECT_EQ(calSensor.getCalibrationState(), CalibrationState::HARD_IRON_ESTIMATED);
    
    // Verify compensated field is close to Earth field
    sf::Vec3 compensated;
    ASSERT_TRUE(calSensor.readCalibratedMag(compensated));
    
    float earthMag = std::sqrt(25*25 + 40*40);  // ~47 µT
    float measuredMag = compensated.magnitude();
    EXPECT_NEAR(measuredMag, earthMag, 2.0f);  // Within 2 µT
}
```

### 5.2 Test: Disturbance Detection

```cpp
TEST(MagDisturbance, DetectsNearbyFerromagnetic) {
    // Setup environment with nearby disturbance
    MagneticEnvironment env;
    env.setEarthField(EarthFieldModel{25.0f, 40.0f, 0, 60});
    
    // Add laptop-sized dipole at 0.5m
    DipoleSource laptop;
    laptop.position = {0.5f, 0, 0};
    laptop.moment = {0, 0, 0.5f};  // Vertical moment
    laptop.decayRadius = 2.0f;
    env.addDipole(laptop);
    
    BMM350Simulator sensor;
    CalibratedMagSensor calSensor(sensor, env, {0, 0, 0});
    calSensor.setCalibrationState(CalibrationState::FIELD_VALIDATED);
    
    // Read sensor at origin
    sf::Vec3 field;
    ASSERT_TRUE(calSensor.readCalibratedMag(field));
    
    // Should detect disturbance
    float disturbanceIndicator = calSensor.getDisturbanceIndicator();
    EXPECT_GT(disturbanceIndicator, 0.5f);  // Significant disturbance
    
    // Verify field quality degraded
    auto quality = env.getQualityAt({0, 0, 0});
    EXPECT_TRUE(quality.isDisturbed);
    EXPECT_GT(quality.disturbanceRatio, 0.3f);
}
```

### 5.3 Test: Scenario-Based Validation

```cpp
TEST(MagScenario, WearableWalkingMotion) {
    auto scenario = DisturbanceScenario::wearableWalking();
    
    VirtualGimbal gimbal;
    BMM350Simulator sensor;
    CalibratedMagSensor calSensor(sensor, scenario.env, {0, 0, 0});
    
    // Execute motion profile
    float totalError = 0;
    int sampleCount = 0;
    
    for (const auto& segment : scenario.motion) {
        float dt = 0.02f;  // 50 Hz
        for (float t = 0; t < segment.durationSec; t += dt) {
            gimbal.setRotationRate(segment.rotationAxis * segment.rotationRateDps);
            gimbal.update(dt);
            gimbal.syncToSensors();
            
            sf::Vec3 measured, expected;
            calSensor.readCalibratedMag(measured);
            expected = scenario.env.getFieldAt(/* current position */);
            
            float error = (measured - expected).magnitude();
            totalError += error;
            sampleCount++;
        }
    }
    
    float avgError = totalError / sampleCount;
    EXPECT_LT(avgError, scenario.expected.maxHeadingErrorDeg);
}
```

### 5.4 Test: AHRS Robustness to Disturbance

```cpp
TEST(MagAHRS, MaintainsHeadingDuringTemporaryDisturbance) {
    MagneticEnvironment env = MagneticEnvironment::cleanLab();
    
    // Setup sensor assembly
    VirtualSensorAssembly assembly;
    assembly.magSim().setEarthField({25.0f, 0, -40.0f});
    ASSERT_TRUE(assembly.initAll());
    
    MocapNodePipeline pipeline(assembly.imuDriver(), 
                               &assembly.magDriver(),
                               &assembly.baroDriver());
    
    // Set initial orientation
    assembly.resetAndSync();
    
    // Record initial heading
    MocapNodeSample initialSample;
    pipeline.step(initialSample);
    float initialHeading = quaternionToHeading(initialSample.orientation);
    
    // Add temporary disturbance
    DipoleSource interference;
    interference.position = {0.1f, 0, 0};  // Very close
    interference.moment = {5.0f, 0, 0};     // Strong
    env.addDipole(interference);
    
    // Run for 2 seconds with disturbance
    for (int i = 0; i < 100; ++i) {
        assembly.gimbal().update(0.02f);
        assembly.gimbal().syncToSensors();
        pipeline.step(initialSample);
    }
    
    // Remove disturbance
    env.clearDipoles();
    
    // Run for 3 more seconds to recover
    for (int i = 0; i < 150; ++i) {
        assembly.gimbal().update(0.02f);
        assembly.gimbal().syncToSensors();
        pipeline.step(initialSample);
    }
    
    // Verify heading returned to near initial
    float finalHeading = quaternionToHeading(initialSample.orientation);
    float headingError = std::abs(finalHeading - initialHeading);
    if (headingError > 180) headingError = 360 - headingError;
    
    EXPECT_LT(headingError, 15.0f);  // Within 15° after recovery
}
```

---

## 6. Implementation Tasks

| Task | File | Priority | Est. Effort |
|------|------|----------|-------------|
| 1. MagneticEnvironment | `simulators/magnetic/MagneticEnvironment.hpp/cpp` | 🔴 High | 4h |
| 2. Dipole field model | Unit tests for field calculations | 🔴 High | 2h |
| 3. CalibratedMagSensor | `simulators/magnetic/CalibratedMagSensor.hpp/cpp` | 🔴 High | 6h |
| 4. Calibration algorithms | Hard/soft iron estimation | 🔴 High | 4h |
| 5. DisturbanceScenario | `simulators/magnetic/DisturbanceScenario.hpp/cpp` | 🟡 Medium | 3h |
| 6. Hard iron tests | `simulators/tests/test_mag_hard_iron.cpp` | 🔴 High | 2h |
| 7. Disturbance tests | `simulators/tests/test_mag_disturbance.cpp` | 🔴 High | 3h |
| 8. Scenario tests | `simulators/tests/test_mag_scenarios.cpp` | 🟡 Medium | 4h |
| 9. AHRS robustness | `simulators/tests/test_ahrs_mag_robustness.cpp` | 🟡 Medium | 3h |

**Total Estimated Effort:** ~31 hours

---

## 7. Calibration Quality Metrics

### 7.1 Observable Metrics (for test assertions)

| Metric | Definition | Target |
|--------|------------|--------|
| **Field Magnitude Error** | \|\|B_measured\| - \|B_earth\|\| | < 5 µT |
| **Heading Error** | arctan2(By, Bx) difference from truth | < 5° |
| **Dip Angle Error** | arctan2(Bz, sqrt(Bx²+By²)) difference | < 5° |
| **Coverage Score** | Fraction of sphere sampled during cal | > 80% |
| **Stability Variance** | Variance of compensated field over 10s | < 1 µT² |

### 7.2 Calibration State Machine

```
UNCALIBRATED ──(start cal)──> CALIBRATING
                                    │
                                    ▼
                    ┌───────────────────────────┐
                    │  Collect samples covering │
                    │  >50% of sphere surface   │
                    └───────────────────────────┘
                                    │
                                    ▼
                         HARD_IRON_ESTIMATED
                                    │
                    ┌───────────────┴───────────────┐
                    │  Ellipsoid fit converges      │
                    │  condition number < 10        │
                    └───────────────────────────────┘
                                    │
                                    ▼
                          SOFT_IRON_FULL
                                    │
                    ┌───────────────┴───────────────┐
                    │  Validated against known      │
                    │  field direction              │
                    └───────────────────────────────┘
                                    │
                                    ▼
                           FIELD_VALIDATED
```

---

## 8. References

- BMM350 datasheet: Magnetic specifications, OTP calibration
- Soft iron compensation: `docs/research/mag_calibration_algorithms.md`
- Existing simulator: `simulators/sensors/Bmm350Simulator.hpp`
- AHRS integration: `external/SensorFusion/middleware/ahrs/MahonyAHRS.cpp`
