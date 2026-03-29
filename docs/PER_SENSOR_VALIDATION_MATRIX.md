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

## Current Status

The standalone host test suite now covers the matrix items above for all three
simulated sensors:

1. identity and probe behavior
2. driver-compatible register access
3. physically plausible measurements across known orientations or conditions
4. bias, scale, and noise injection
5. deterministic seeded behavior for quantitative tests

Recent Wave B closure added:

1. LPS22DF cold/hot temperature and below-sea-level checks
2. LSM6DSO stationary gyro, multi-axis gyro, accel-norm, and gyro-noise tests
3. BMM350 pitch-field projection, hard-iron constancy, and noise-stat tests

## Current Codex Focus

1. keep the matrix green while Wave B evidence tooling grows
2. prefer already-supported simulator behavior before introducing new simulator
   features
3. treat new validation work as regression-proofing, not as open-ended simulator
   expansion
