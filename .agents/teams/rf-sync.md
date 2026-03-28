# RF And Sync Team Charter

## Mission

Design and validate the low-latency node-to-master communication and timing
model for HelixDrift before real RF hardware exists.

This team does not own spatial localization. It owns timing correctness,
transport behavior, protocol shape, and simulation of wireless impairments.

## Core Questions

1. What timing model should nodes and the master share?
2. What synchronization method is realistic on a low-power MCU such as nRF52?
3. What packet shape and update cadence minimize latency while preserving
   useful mocap fidelity?
4. How much jitter, loss, reorder, and anchor delay can the system tolerate?
5. What must be proven in host simulation before any radio implementation is
   attempted?

## Deliverables

### D1. Timing Contract

Produce a short design note covering:

- node-local time vs master time
- anchor semantics
- timestamp mapping rules
- startup and resynchronization behavior
- acceptable skew and drift budget

### D2. Protocol Sketch

Produce a first-pass protocol design covering:

- frame types
- sync packets
- mocap data packets
- health/control packets
- retransmission or no-retransmission policy
- sequencing and loss handling

### D3. Host Simulation Harness

Create or extend host-only simulation for:

- fixed latency
- variable latency
- jitter
- packet loss
- out-of-order delivery
- delayed or missing sync anchors

### D4. Validation Metrics

Define and report:

- node-to-master skew in microseconds
- frame age at master receive time
- packet loss rate
- reorder count
- resynchronization time after disturbance

## Non-Goals

- Do not claim node relative position or absolute position.
- Do not own fusion algorithm correctness.
- Do not own hardware RF implementation details until host validation exists.

## Recommended Write Scope

- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/TimestampSynchronizedTransport.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/MocapBleSender.hpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/firmware/common/MocapBleSender.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/test_timestamp_synchronized_transport.cpp`
- `/home/mrumoy/sandbox/embedded/HelixDrift/tests/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/simulators/`
- `/home/mrumoy/sandbox/embedded/HelixDrift/docs/`

## First Task List

1. Write `docs/rf-sync-requirements.md` with timing and latency requirements.
2. Add a `VirtualMasterClock` abstraction and anchor-source simulator.
3. Add transport impairment wrappers for latency, jitter, loss, and reorder.
4. Add sync regression tests using multiple virtual nodes.
5. Propose a packet contract suitable for nRF52-first implementation.

## Success Criteria

- A simulated multi-node system stays within a declared timing budget.
- Transport failure modes are measured and visible.
- The protocol shape is simple enough to implement on nRF52.
- Platform implementation can begin without changing the validated host
  timing contract.
