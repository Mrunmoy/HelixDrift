# Simulation-First Backlog

This document turns the HelixDrift product goal into an executable backlog.

The product to validate is a small wearable mocap sensor node using an IMU,
magnetometer, and barometer, synchronized to a master node with low latency.
Until real hardware exists, the main job of this repository is to prove the
system in simulation.

## Validation Order

1. Prove each sensor independently.
2. Prove the three-sensor assembly on a single node.
3. Prove host-side node runtime behavior using the common abstractions.
4. Prove multi-node timestamp alignment against a master-node timebase.
5. Prove useful relative-body measurements from multiple synchronized nodes.
6. Port the validated runtime to concrete MCU targets.

## Milestone 1: Per-Sensor Proof

Goal: each simulated sensor and its real driver path is proven independently
before any three-sensor fusion claim is trusted.

Acceptance criteria:

- The IMU, magnetometer, and barometer each have standalone validation tests.
- Each sensor passes probe/init/register access tests through the real driver.
- Each sensor has quantitative checks for the physical behaviors it is
  expected to represent.
- Error injection is testable per sensor in isolation.
- Sensor-specific regression failures are distinguishable from fusion failures.

Implementation targets:

- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/tests/test_lsm6dso_simulator.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/tests/test_bmm350_simulator.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/tests/test_lps22df_simulator.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/i2c/VirtualI2CBus.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/i2c/VirtualI2CBus.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/external/SensorFusion/drivers/lsm6dso/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/external/SensorFusion/drivers/bmm350/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/external/SensorFusion/drivers/lps22df/`

Suggested tasks:

- Add a per-sensor validation matrix document with expected input/output
  behavior and tolerances.
- Separate driver-init regressions from sensor-physics regressions.
- Add more tests for register-level edge cases and scale conversion.

Sensor-specific proof expectations:

- IMU:
  validate accelerometer gravity direction, gyro rate scaling, temperature,
  bias, scale error, and noise.
- Magnetometer:
  validate earth-field projection, orientation response, hard iron, soft iron,
  OTP/init behavior, and temperature.
- Barometer:
  validate sea-level pressure, altitude response, temperature, bias, and noise.

## Milestone 2: Single-Node Sensor Assembly Proof

Goal: a virtual sensor assembly moved through known motions yields bounded,
repeatable fused orientation output.

Acceptance criteria:

- `./build.py --clean --host-only -t` stays green.
- Simulator-backed tests cover static pose, constant angular rate, oscillation,
  compound rotation, and full-turn return-to-origin motion.
- Tests assert quantitative orientation thresholds instead of only
  pass/fail sanity checks.
- Error injection for gyro bias, accelerometer bias, magnetic hard/soft iron,
  and barometric noise causes measurable and expected degradation.

Implementation targets:

- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/tests/test_sensor_fusion_integration.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/gimbal/VirtualGimbal.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/gimbal/VirtualGimbal.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/sensors/Lsm6dsoSimulator.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/sensors/Lsm6dsoSimulator.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/sensors/Bmm350Simulator.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/sensors/Bmm350Simulator.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/sensors/Lps22dfSimulator.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/sensors/Lps22dfSimulator.cpp`

Suggested tasks:

- Add helpers for computing angular error between expected and recovered
  quaternions.
- Add motion-script fixtures so tests are concise and reusable.
- Convert broad sanity assertions into bounded numeric assertions.

## Milestone 3: Host-Side Virtual Node Harness

Goal: run the common HelixDrift node behavior end-to-end on host using
simulator inputs and a fake transport.

Acceptance criteria:

- A host-only test can instantiate a virtual node using simulator sensors,
  real SensorFusion drivers, `MocapNodeLoop`, and a fake outgoing transport.
- Output cadence matches the selected profile over simulated time.
- Timestamped quaternion frames and health frames can be captured for
  assertions or offline analysis.
- Pipeline failure, dropped samples, and delayed anchor updates are testable.

Implementation targets:

- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/MocapNodeLoop.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/MocapProfiles.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/MocapHealthTelemetry.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/TimestampSynchronizedTransport.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/test_mocap_node_loop.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/test_timestamp_synchronized_transport.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/tests/`

Suggested tasks:

- Add a `VirtualClock` and a `CaptureTransport` test utility.
- Add a host-only virtual-node integration test file under `tests/` or
  `simulators/tests/`.
- Add deterministic frame-capture logs for regression comparisons.

## Milestone 4: Master-Node Time Sync Simulation

Goal: prove that multiple nodes can map their local timestamps into a master
timebase with bounded error under realistic sync behavior.

Acceptance criteria:

- A host-side master-node simulator can emit sync anchors.
- Multiple virtual nodes can consume anchors and map local sample timestamps
  into a shared master timebase.
- Tests quantify sync error under nominal conditions.
- Tests cover anchor loss, delayed anchors, and clock offset/drift.

Implementation targets:

- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/TimestampSynchronizedTransport.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/test_timestamp_synchronized_transport.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/`

Suggested tasks:

- Add a reusable sync-filter fake or reference implementation for tests.
- Add a `VirtualMasterClock` and `AnchorSource` simulator.
- Add explicit error-budget assertions, for example max skew in microseconds.

## Milestone 5: Network Impairment Model

Goal: validate the streaming and synchronization model under the wireless
conditions that will matter on-body.

Acceptance criteria:

- Host-side tests can inject fixed latency, jitter, loss, duplication, and
  out-of-order delivery.
- Node output remains decodable and time-aligned within defined budgets.
- Failure modes are visible and bounded rather than silent.

Implementation targets:

- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/MocapBleSender.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/MocapBleSender.cpp`

Suggested tasks:

- Add a transport wrapper that schedules packets through an impairment queue.
- Add test scenarios that compare ideal vs impaired timing alignment.
- Record per-frame latency and reorder statistics for assertions.

## Milestone 6: Multi-Node Body Kinematics

Goal: show that synchronized nodes can recover useful relative body motion,
starting with joint-angle estimation rather than unconstrained absolute
position claims.

Acceptance criteria:

- Two-node tests can recover relative orientation and a joint angle from known
  scripted motion.
- Three-node tests can recover a simple body-chain motion with bounded timing
  skew and bounded angular error.
- Test outputs are suitable for plotting or export to CSV for inspection.

Implementation targets:

- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/gimbal/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/tests/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/`

Suggested tasks:

- Add a multi-rig or multi-node simulation harness.
- Add reference body-segment motion scripts.
- Add assertions for elbow-like or knee-like hinge motion recovery.

## Milestone 7: Platform Port of Validated Runtime

Goal: keep MCU-specific work thin and port only what has already been proven,
with nRF52 as the primary target path.

Acceptance criteria:

- nRF52 board code initializes sensors, runs the validated common loop, and
  streams timestamped outputs.
- OTA, health telemetry, and sync flow integrate without changing the
  host-validated contracts.
- On-target validation uses the same scenarios already proven in simulation.

Implementation targets:

- `/home/mrumoy/sandbox/embedded/HelixDrift/examples/nrf52-mocap-node/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/docs/validation/ON_TARGET_VALIDATION.md`
- `/home/mrumoy/sandbox/embedded/HelixDrift/docs/validation/TEST_LOG_TEMPLATE.md`

## Metrics To Track

These should become explicit outputs of tests and validation reports:

- orientation max error in degrees
- orientation RMS error in degrees
- drift after fixed-duration static hold
- drift after full-rotation return-to-origin motion
- master-node sync skew in microseconds
- end-to-end frame latency in milliseconds
- packet loss and reordering counts in impaired simulations

## Immediate Next Tasks

1. Add a per-sensor validation matrix and close any remaining single-sensor
   tolerance gaps before expanding fusion work.
2. Add a virtual sensor-assembly host test harness that composes the three
   proven sensors into one node fixture.
3. Add a virtual-node host test harness that composes simulator sensors with
   `MocapNodeLoop` and a capture transport.
4. Add reusable quaternion-error helpers and assert actual angular thresholds.
5. Add a virtual master-node timebase and anchor source for sync tests.
6. Add a transport impairment queue for latency, jitter, and loss testing.
7. Add the first two-node kinematic scenario before spending more effort on
   platform-specific example code.
