# Simulator Development Journal

## 2026-03-29 - Day 1

### Work Done
1. **Architecture Planning** (with user)
   - Decided on 3 mocap sensors only (LSM6DSO, BMM350, LPS22DF)
   - Level B fidelity (noise, bias, scale errors)
   - Single-process architecture
   - Option B directory structure

2. **Project Setup**
   - Created `simulators/` directory structure
   - Created branch `feature/sensor-simulators`
   - Added README.md, TASKS.md

3. **VirtualI2CBus Implementation (TDD)**
   - Header with I2CDevice interface
   - Transaction logging support
   - Implementation of read/write/probe
   - Comprehensive unit tests

4. **Build System Integration**
   - Added `helix_simulators` library to CMakeLists.txt
   - Added `helix_integration_tests` executable

### Tests Status
- 12 tests written for VirtualI2CBus
- 0 tests passing (need to build first)

### Blockers
- None

### Next Steps
1. Build and verify VirtualI2CBus tests pass
2. First commit: VirtualI2CBus + tests
3. Start LSM6DSO simulator

### Notes
- Need to include <map> in test file for MockI2CDevice
- CMake needs to link integration tests against helix_simulators
