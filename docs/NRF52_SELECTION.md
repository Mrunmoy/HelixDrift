# nRF52 MCU Selection Note

This document records the current MCU selection guidance for HelixDrift.

## Current Recommendation

Use `nRF52840` as the primary development and first-product MCU target.

Reason:

- enough memory headroom for sensor drivers, fusion, transport logic, DFU, and
  debugging during early product development
- strong low-power Bluetooth and 2.4 GHz ecosystem
- flexibility to prototype Bluetooth LE, proprietary 2.4 GHz, and 802.15.4
  based communication strategies
- already available in-hand for development

## Why nRF52840 First

The main value of `nRF52840` is not that it is the smallest or cheapest. The
value is that it gives enough room to de-risk the product without premature
resource pressure.

For HelixDrift v1, the node MCU likely needs to support:

- IMU, magnetometer, and barometer drivers
- sensor fusion and calibration logic
- health telemetry
- time synchronization logic
- low-latency packet transport
- firmware update path
- development diagnostics and temporary instrumentation

The `nRF52840` is a good first choice because it reduces the chance that the
team wastes time fighting memory pressure while still proving the system.

## Recommended Positioning In The Plan

- `nRF52840` is the primary target for simulation-to-hardware transition.
- `nRF52833` is a later optimization candidate if memory and peripheral usage
  stay well below `nRF52840` limits.
- `nRF5340` is a later escalation candidate if the node software outgrows the
  practical limits of a single-core nRF52 design.

## Decision Table

### Use nRF52840 now when:

- the priority is proving the system rather than minimizing BOM
- DFU, logging, protocol experiments, and instrumentation are still evolving
- there is uncertainty around final protocol and feature footprint
- one available dev board already exists

### Consider moving down to nRF52833 later when:

- memory usage is comfortably below `nRF52840` limits
- the protocol and feature set have stabilized
- USB is no longer needed
- cost, PCB area, or module selection pressure matters more than development
  headroom

### Consider moving up to nRF5340 later when:

- application-side CPU load becomes difficult to manage
- protocol stack and application responsibilities need stronger isolation
- memory pressure becomes persistent
- multi-node timing, buffering, crypto, or future features clearly exceed the
  practical comfort zone of `nRF52840`

## What To Measure Before Reconsidering

Do not switch MCUs on intuition alone. Track these:

- flash usage in realistic builds
- RAM usage in realistic runtime scenarios
- average and peak CPU load
- radio duty cycle and timing slack
- end-to-end latency under expected node traffic
- power consumption during representative workloads

## Practical Trigger Rules

These are engineering triggers, not hard product requirements.

### Stay on nRF52840 if:

- flash usage remains comfortably below the device limit
- RAM usage leaves practical debug and feature headroom
- timing deadlines are met
- power is acceptable for the target battery life

### Re-evaluate toward nRF52833 if:

- the final firmware is consistently lightweight
- RAM and flash margins remain generous
- required peripherals and radio behavior fit without compromises

### Re-evaluate toward nRF5340 if:

- instrumentation has already been removed and the node is still resource-tight
- timing margin is consistently poor
- protocol and application complexity keep growing

## What This Decision Does Not Solve

Choosing `nRF52840` does not solve:

- whether orientation alone is sufficient for the mocap product
- whether relative node translation is required
- how node-to-master synchronization should work
- whether custom proprietary RF is better than BLE for the final system

Those remain system-level questions for the RF/Sync and Pose Inference teams.

## Current Project Rule

Until measurements justify changing direction:

- primary intended MCU target: `nRF52840`
- primary automated validation target: host simulation and host tests
- platform-specific implementation should remain secondary to proving the
  system in simulation
