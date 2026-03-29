# SensorFusion Init Convention Bug

## Summary

`MahonyAHRS::initFromSensors()` in the SensorFusion submodule partially fixes
large-yaw startup, but pitch and roll initialization still appear to use a
convention that does not match the simulator's orientation model.

## Evidence

With the current merged submodule pointer (`214c28a`) and clean sensors:

- identity: near `0 deg`
- static `+15 deg` yaw: about `15 deg RMS`, `18.6 deg max`
- static `+15 deg` pitch: about `29 deg RMS`, `29.9 deg max`
- static `+15 deg` roll: about `37.8 deg RMS`, `40.7 deg max`
- dynamic yaw at `30 deg/s`: bounded but weak (`~23.6 deg RMS`)
- dynamic pitch/roll at `30 deg/s`: diverge to about `180 deg`

This pattern points to a convention mismatch rather than a simple gain issue:

- yaw is roughly in the right ballpark
- pitch is about `2x` wrong
- roll is about `2.5x` wrong and gets worse under filter correction

## Reproduction Target

The next SensorFusion repro should be a direct init-only check:

1. synthesize accel + mag for a known `+15 deg` pitch orientation
2. call `MahonyAHRS::initFromSensors(accel, mag)`
3. compare the initialized quaternion against
   `Quaternion::fromAxisAngle(0, 1, 0, 15)`
4. require angular error under `1 deg`

The same pattern should be checked for `+15 deg` roll.

## Current HelixDrift Handling

Until this is fixed in SensorFusion:

- keep `A1a` and `A2` yaw-only
- do not add fake pitch/roll thresholds
- treat pitch/roll startup and tracking as blocked on the submodule fix
