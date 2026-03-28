# Per-Sensor Validation Matrix

This document defines what it means for each simulated sensor in HelixDrift to
be proven independently before three-sensor fusion results are trusted.

## Purpose

Fusion-level tests are only credible if each sensor and its driver-facing
behavior has already been validated in isolation.

This matrix separates:

- device identity and probe behavior
- register read or write behavior
- physical measurement behavior
- error injection behavior
- reproducibility requirements

## Global Standalone Criteria

Every sensor simulator should satisfy these categories:

1. identity and probe
2. driver-compatible register access
3. physically plausible measurement output
4. configurable bias, scale, and noise where applicable
5. deterministic behavior when seeded for quantitative tests

## LSM6DSO IMU

### Identity And Register Behavior

- `WHO_AM_I` returns the correct device ID
- probe succeeds on the virtual bus
- control registers retain written values where appropriate
- configured accel and gyro full-scale selections change raw counts
  consistently for the same physical input

### Physical Measurement Behavior

- accelerometer reports gravity in the expected sensor axis for known
  orientations
- gyroscope reports expected angular rate in physical units
- temperature output remains realistic and configurable

### Error Injection

- accelerometer bias changes output by the expected amount
- gyroscope bias changes output by the expected amount
- accelerometer and gyroscope scale factors change output by the expected amount
- accelerometer and gyroscope noise produce measurable variation consistent with
  configured standard deviation

### Reproducibility

- when seeded, repeated runs with the same setup should produce identical noisy
  sample sequences

## BMM350 Magnetometer

### Identity And Register Behavior

- chip ID matches the expected value
- probe succeeds
- configuration and OTP-related register flows remain driver-compatible

### Physical Measurement Behavior

- the earth field projects into the correct sensor axes for known orientations
- custom earth-field configuration changes output predictably
- temperature output remains realistic

### Error Injection

- hard iron offsets shift output by expected additive amounts
- soft iron scaling changes output by expected multiplicative amounts
- bias and noise are observable and bounded

### Reproducibility

- when seeded, repeated noisy runs with the same setup should match

## LPS22DF Barometer

### Identity And Register Behavior

- `WHO_AM_I` returns the correct device ID
- probe succeeds
- writable control registers retain expected values
- software reset clears writable control registers as expected

### Physical Measurement Behavior

- pressure at sea level is correct within tolerance
- pressure decreases with increasing altitude
- direct pressure override works
- temperature output remains realistic and configurable

### Error Injection

- pressure bias shifts output by expected additive amount
- pressure noise produces measurable variation close to configured standard
  deviation
- temperature bias shifts output by expected amount

### Reproducibility

- when seeded, repeated noisy pressure runs with the same setup should match

## Current High-Value Gaps

Deterministic seeding is now exposed explicitly across all three sensors and is
covered by standalone tests. The next bounded gaps are:

1. stronger register-behavior proof for simulated device reset and configuration
2. broader quantitative checks that tie configured sensor scale or calibration
   settings to raw output counts
3. tighter statistical validation of configured noise behavior without making
   the tests brittle

## Current Codex Focus

1. keep converting standalone matrix items into explicit host tests
2. prefer already-supported simulator behavior before introducing new simulator
   features
3. stay within the Sensor Validation scope until the next planning handoff
