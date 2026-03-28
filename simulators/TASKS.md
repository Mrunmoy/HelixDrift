# Simulator Tasks

## Active Sprint

### In Progress
- [ ] Virtual I2C Bus implementation
  - [x] Core bus routing
  - [x] Transaction logging
  - [x] Unit tests
  - [ ] Multi-bus support (I2C0 + I2C1)

### Next Up
- [ ] LSM6DSO Simulator
  - [ ] Register map implementation
  - [ ] Accel/gyro data generation from orientation
  - [ ] Configurable noise/bias
  - [ ] Temperature sensor
  - [ ] Data-ready interrupt simulation
  
- [ ] BMM350 Simulator
  - [ ] Register map implementation
  - [ ] Mag data generation
  - [ ] OTP data simulation
  - [ ] Temperature compensation
  - [ ] Hard/soft iron injection
  
- [ ] LPS22DF Simulator
  - [ ] Register map implementation
  - [ ] Pressure/temperature data
  - [ ] Altitude calculation

- [ ] Virtual Gimbal
  - [ ] Quaternion-based orientation
  - [ ] Rotation rate API
  - [ ] Motion script loader (JSON)
  - [ ] Physics dynamics (optional)

- [ ] I2C EEPROM Simulator
  - [ ] AT24Cxx compatible
  - [ ] Page write simulation
  - [ ] Endurance simulation

- [ ] Integration Tests
  - [ ] Sensor initialization sequence
  - [ ] Calibration flow testing
  - [ ] Sensor fusion validation
  - [ ] Motion invariant tests

### Done
- [x] Project structure setup
- [x] VirtualI2CBus header/implementation
- [x] First TDD test for VirtualI2CBus
- [x] CMake integration

## Design Decisions Log

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-03-29 | Single-process architecture | Simpler debugging, no IPC overhead, fast tests |
| 2026-03-29 | Level B fidelity (noise/bias) | Enables calibration testing without complexity |
| 2026-03-29 | Option B directory structure | Clean separation of concerns |
| 2026-03-29 | Separate integration test executable | Distinct from unit tests, different scope |
