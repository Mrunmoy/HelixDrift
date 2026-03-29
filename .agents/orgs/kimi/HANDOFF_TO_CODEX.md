# Handoff: RF/Sync and Magnetic Implementation Slices

**From:** Kimi (research/spec → implementation-support)  
**To:** Codex (implementation)  
**Date:** 2026-03-29  
**Context:** Claude handling architecture sequencing

---

## Summary

Kimi has completed the spec-to-slices breakdown for both RF/Sync and Magnetic/Calibration work. This document provides Codex with everything needed to begin implementation without ambiguity.

---

## Documents Delivered

| Document | Location | Purpose |
|----------|----------|---------|
| RF Implementation Slices | `.agents/orgs/kimi/RF_SYNC_IMPLEMENTATION_SLICES.md` | 6 ranked slices, detailed breakdown |
| Magnetic Implementation Slices | `.agents/orgs/kimi/MAGNETIC_IMPLEMENTATION_SLICES.md` | 5 ranked slices, detailed breakdown |
| Kimi Org Status | `.agents/orgs/kimi/ORG_STATUS.md` | Planning gate compliance |
| This Handoff | `.agents/orgs/kimi/HANDOFF_TO_CODEX.md` | Quick reference |

---

## Recommended Starting Points

### Start Here: RF Slice 1 (VirtualRFMedium Core)

```
Files: simulators/rf/VirtualRFMedium.hpp
       simulators/rf/VirtualRFMedium.cpp
       simulators/tests/test_rf_medium_basic.cpp (new)

Effort: 2-3 hours
Risk: Low (pure new code, no existing dependencies)
Tests: 3 tests - latency, broadcast, packet loss
```

**Why first:** Unblocks all other RF work. No risk to existing 220 passing tests.

### Then: Magnetic Slice 1 (MagneticEnvironment Core)

```
Files: simulators/magnetic/MagneticEnvironment.hpp
       simulators/magnetic/MagneticEnvironment.cpp
       simulators/tests/test_magnetic_environment.cpp (new)

Effort: 2-3 hours
Risk: Low (pure new code, no existing dependencies)
Tests: 3 tests - uniformity, dipole decay, preset environments
```

**Why second:** Parallelizable with RF work (different directories). No conflicts.

---

## What NOT to Do (Anti-Scope-Creep)

### RF Work
- ❌ Don't modify `VirtualMocapNodeHarness` (Claude owns integration)
- ❌ Don't modify `VirtualSensorAssembly` (risk of breaking 220 tests)
- ❌ Don't add I2C timing (future milestone)
- ❌ Don't add crypto/security (out of scope for v1)

### Magnetic Work
- ❌ Don't modify `MahonyAHRS` (Claude owns fusion)
- ❌ Don't modify `MocapNodePipeline` (Claude owns pipeline)
- ❌ Don't add soft iron (v2 feature)
- ❌ Don't add temperature model (future work)
- ❌ Don't add moving sensors (kinematics work, later)

### General
- ❌ Don't merge slices - implement one at a time
- ❌ Don't skip tests - TDD required per `.agents/README.md`
- ❌ Don't edit `.agents/` files (Claude owns org management)

---

## Dependency Map

```
RF Slices:
  Slice 1: VirtualRFMedium (START HERE)
    ↓
  Slice 2: ClockModel + VirtualSyncNode
    ↓
  Slice 3: VirtualSyncMaster + Anchors
    ↓
  Slice 4: Loss Robustness
    ↓
  Slice 5: Multi-Node
    ↓
  Slice 6: Drift Tracking (DEFER - optimization)

Magnetic Slices:
  Slice 1: MagneticEnvironment (START HERE)
    ↓
  Slice 3: HardIronCalibrator (can parallelize)
    ↓
  Slice 2: BMM350 Integration (low risk)
    ↓
  Slice 4: CalibratedMagSensor (DEFER - high integration risk)
    ↓
  Slice 5: AHRS Robustness (DEFER - wait for RF stable)
```

---

## Test Requirements

Per `.agents/README.md` global rules:
1. Design first (slice doc provides this)
2. **Then TDD** (tests first)
3. Then implement and run unit tests
4. Then fix issues

Each slice includes:
- First tests to write (copy-paste ready)
- Measurable outputs (checkboxes)
- Explicit out-of-scope items

---

## Review Routing

Per `.agents/MODEL_ASSIGNMENT.md`:
1. **Codex implements** (you)
2. **Kimi reviews** implementation against spec
3. **Claude reviews** architecture alignment
4. **Systems Architect** final signoff

When you complete a slice:
1. Update `simulators/docs/DEV_JOURNAL.md`
2. Request review from Kimi
3. Request review from Claude
4. Merge after both approvals

---

## Questions?

If during implementation you encounter:
- **Spec ambiguity:** Ask Kimi for clarification
- **Architecture conflict:** Ask Claude for sequencing decision
- **Scope creep temptation:** Check this handoff doc
- **Integration block:** Escalate to Systems Architect

---

## Quick Reference: First Test Files

```cpp
// simulators/tests/test_rf_medium_basic.cpp
#include <gtest/gtest.h>
#include "VirtualRFMedium.hpp"

using namespace sim;

TEST(VirtualRFMedium, SinglePacketDeliveredWithLatency) {
    VirtualRFMedium medium({.baseLatencyUs = 500});
    bool received = false;
    medium.registerNode(1, [&](const Packet&, uint64_t) { received = true; });
    
    Packet p{.srcId = 2, .dstId = 1, .payload = {1, 2, 3}};
    medium.transmit(2, p);
    
    EXPECT_FALSE(received);
    medium.advanceTimeUs(500);
    EXPECT_TRUE(received);
}
```

```cpp
// simulators/tests/test_magnetic_environment.cpp
#include <gtest/gtest.h>
#include "MagneticEnvironment.hpp"

using namespace sim;

TEST(MagneticEnvironment, EarthFieldUniformEverywhere) {
    MagneticEnvironment env;
    env.setEarthField({25.0f, 40.0f, 0.0f});
    
    Vec3 atOrigin = env.getFieldAt({0, 0, 0});
    Vec3 at1Meter = env.getFieldAt({1, 0, 0});
    
    EXPECT_NEAR(atOrigin.x, at1Meter.x, 0.01f);
    EXPECT_NEAR(atOrigin.magnitude(), 47.2f, 0.1f);
}
```

---

**Status:** ✅ Ready for Codex implementation
**Estimated Total Effort:** 31h (RF) + 16h (Magnetic) = 47h
**Recommended Parallel Track:** RF Slice 1 + Magnetic Slice 1 simultaneously
