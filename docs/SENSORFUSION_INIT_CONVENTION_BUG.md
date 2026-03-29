# SensorFusion AHRS Convention Fix

## Summary

The earlier pitch/roll startup failure was not just an
`initFromSensors()` problem. The full issue was a broader AHRS convention
mismatch inside the SensorFusion submodule:

- `initFromSensors()` seeded yaw correctly but used a tilt reconstruction path
  that did not match the simulator quaternion convention
- `update()` and `update6DOF()` predicted gravity and magnetic references using
  formulas that did not match `Quaternion::rotateVector()`
- the 9DOF magnetometer error term also used the opposite cross-product
  direction for yaw correction

Together, those mismatches produced:

- reasonable yaw startup
- pitch/roll startup errors
- pitch/roll divergence after repeated stationary updates
- misleading Helix-side characterization about which axes and gains were
  "harder"

## Repro That Caught It

These submodule tests now exist and pass:

1. `MahonyAHRSTest.InitFromSensorsSeedsSmallPitchOffsetCloseToTruth`
2. `MahonyAHRSTest.InitFromSensorsSeedsSmallRollOffsetCloseToTruth`
3. `MahonyAHRSTest.InitFromSensorsMaintainsSmallPitchPoseUnderStationaryUpdates`
4. `MahonyAHRSTest.InitFromSensorsMaintainsSmallRollPoseUnderStationaryUpdates`

The direct hold-after-init tests were the critical boundary: they proved that
the remaining bug lived in `update()` after a correct seed, not in the Helix
harness.

## Fix Applied In SensorFusion

The submodule fix now does three things:

1. replaces the Euler-based `initFromSensors()` seeding path with basis-aligned
   quaternion construction from accel + mag
2. derives gravity and magnetic predictions in `update()` and `update6DOF()`
   through `Quaternion::rotateVector()` so they share the same frame
   convention as the rest of the codebase
3. flips the 9DOF magnetic cross-product term so yaw correction opposes yaw
   error instead of reinforcing it

## Helix Impact

After the submodule fix:

- small static offsets seed accurately across yaw, pitch, and roll
- yaw-only acceptance stays valid
- long-horizon drift, joint-angle recovery, and bounded yaw tracking remain
  green
- several older Helix characterization tests had to be rewritten because they
  were asserting pre-fix failure modes instead of current behavior

This document is now a resolved bug note, not an open blocker.
