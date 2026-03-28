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
   - Added README.md, TASKS.md, DESIGN.md

3. **VirtualI2CBus Implementation (TDD)** - Commit: 52d525e
   - Header with I2CDevice interface
   - Transaction logging support
   - Implementation of read/write/probe
   - 11 unit tests, all passing

4. **Parallel Sensor Simulator Development**
   - **LSM6DSO Simulator** - Commit: d87c172 (18 tests)
     - Register map: WHO_AM_I, CTRL, gyro/accel data, temp
     - Data generation from orientation + rotation rate
     - Noise, bias, scale error injection
   - **BMM350 Simulator** - Commit: edd5bbc (17 tests)
     - Register map: CHIP_ID, PMU, OTP, mag data, temp
     - Mag data from earth field + orientation
     - Hard/soft iron error injection
   - **LPS22DF Simulator** - Commit: 3816c5e (15 tests)
     - Register map: WHO_AM_I, CTRL, pressure/temp data
     - Barometric pressure from altitude
     - Noise and bias injection

### Tests Status
- **61/61 integration tests passing**
  - 11 VirtualI2CBus
  - 18 LSM6DSO
  - 17 BMM350
  - 15 LPS22DF

### Commits Made
1. 52d525e - VirtualI2CBus with transaction logging
2. d87c172 - LSM6DSO simulator
3. edd5bbc - BMM350 simulator
4. 3816c5e - LPS22DF simulator

### Next Steps
1. VirtualGimbal - coordinate sensor movement
2. I2C EEPROM Simulator - calibration storage
3. Sensor fusion integration tests
4. Motion scripts (JSON)

### Notes
- All sensor simulators implement I2CDevice interface
- Each sensor has setOrientation() for consistent behavior
- Error injection ready for calibration testing
