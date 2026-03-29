# Simulator Design Document

## Goals

1. **Off-Target Testing**: Test sensor fusion algorithms without hardware
2. **Calibration Validation**: Verify calibration algorithms with known error injection
3. **Motion Simulation**: Test with realistic movement patterns
4. **Regression Testing**: Catch integration issues before hardware testing

## Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Integration Test                          │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              VirtualGimbal                           │   │
│  │  - Orientation (quaternion)                          │   │
│  │  - Rotation rates                                    │   │
│  │  - Motion script execution                           │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│                       ▼                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │         Physics Engine (optional)                    │   │
│  │  - Convert orientation to sensor readings            │   │
│  │  - Add noise, bias, scale errors                     │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│         ┌─────────────┼─────────────┐                        │
│         ▼             ▼             ▼                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                   │
│  │ LSM6DSO  │  │ BMM350   │  │ LPS22DF  │  Sensor Simulators│
│  │Simulator │  │Simulator │  │Simulator │                   │
│  │- Registers│  │- Registers│  │- Registers│                  │
│  │- Accel   │  │- Mag     │  │- Pressure│                   │
│  │- Gyro    │  │- OTP data│  │- Temp    │                   │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                   │
│       │             │             │                          │
│       └─────────────┼─────────────┘                          │
│                     ▼                                        │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              VirtualI2CBus (I2C0)                    │   │
│  │  - Routes 0x6A (LSM6DSO)                             │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              VirtualI2CBus (I2C1)                    │   │
│  │  - Routes 0x14 (BMM350), 0x5D (LPS22DF)              │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │           MocapNodePipeline (real code)              │   │
│  │  - Mahony AHRS                                       │   │
│  │  - Calibration flow                                  │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│                       ▼                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Validation Layer                        │   │
│  │  - Reference quaternion comparison                   │   │
│  │  - Motion invariants                                 │   │
│  │  - Output logging                                    │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Virtual I2C Bus

### Interface
```cpp
class VirtualI2CBus : public sf::II2CBus {
    void registerDevice(uint8_t address, I2CDevice& device);
    bool readRegister(uint8_t devAddr, uint8_t reg, uint8_t* buf, size_t len);
    bool writeRegister(uint8_t devAddr, uint8_t reg, const uint8_t* data, size_t len);
    bool probe(uint8_t devAddr);
    
    // Transaction logging for debugging
    void setLoggingEnabled(bool enabled);
    const std::vector<I2CTransaction>& getTransactions() const;
};
```

### Transaction Log
Every I2C transaction is logged with:
- Type (READ/WRITE/PROBE)
- Device address
- Register address
- Data bytes
- Timestamp
- Success/failure

## Sensor Simulators

### Common Interface
All sensor simulators implement `I2CDevice`:
```cpp
class I2CDevice {
    virtual bool readRegister(uint8_t reg, uint8_t* buf, size_t len) = 0;
    virtual bool writeRegister(uint8_t reg, const uint8_t* data, size_t len) = 0;
    virtual bool probe() = 0;
};
```

### LSM6DSO Simulator

**Registers to implement:**
- `0x0F` (WHO_AM_I) - Returns 0x6C
- `0x10` (CTRL1_XL) - Accelerometer config
- `0x11` (CTRL2_G) - Gyro config
- `0x12` (CTRL3_C) - Control register
- `0x20-0x21` (OUTX_L_G/OUTX_H_G) - Gyro X
- `0x22-0x23` (OUTY_L_G/OUTY_H_G) - Gyro Y
- `0x24-0x25` (OUTZ_L_G/OUTZ_H_G) - Gyro Z
- `0x28-0x29` (OUTX_L_XL/OUTX_H_XL) - Accel X
- `0x2A-0x2B` (OUTY_L_XL/OUTY_H_XL) - Accel Y
- `0x2C-0x2D` (OUTZ_L_XL/OUTZ_H_XL) - Accel Z
- `0x20` (OUT_TEMP_L) - Temperature

**Data Generation:**
```
Accel = R * [0, 0, 1g] + bias + noise
Gyro = rotation_rate + bias + noise
```
Where R is rotation matrix from current orientation.

### BMM350 Simulator

**Registers:**
- `0x00` (CHIP_ID) - Returns 0x33
- OTP reading, mag data, temperature compensation

**Mag Data:**
```
Mag = R * earth_field + hard_iron + soft_iron * R * earth_field + noise
```

### LPS22DF Simulator

**Pressure Model:**
```
Pressure = f(altitude) + noise
Altitude from VirtualGimbal position
```

## Virtual Gimbal

### API
```cpp
class VirtualGimbal {
    void setOrientation(const Quaternion& q);
    void setRotationRate(float wx, float wy, float wz);  // rad/s
    void update(float dt);
    
    // Motion scripts
    bool loadMotionScript(const std::string& jsonPath);
    void runMotionScript();
    
    Quaternion getOrientation() const;
    Vec3 getRotationRate() const;
    Vec3 getAccel() const;  // Including gravity
};
```

### Motion Script Format (JSON)
```json
{
  "name": "360 degree yaw rotation",
  "duration_seconds": 10.0,
  "steps": [
    {"time": 0.0, "action": "set_orientation", "params": {"w": 1, "x": 0, "y": 0, "z": 0}},
    {"time": 0.0, "action": "set_rate", "params": {"wx": 0, "wy": 0, "wz": 0.628}},
    {"time": 10.0, "action": "set_rate", "params": {"wx": 0, "wy": 0, "wz": 0}}
  ]
}
```

## Calibration Testing

### Error Injection
```cpp
struct SensorErrors {
    Vec3 bias;
    Vec3 scale;
    Mat3 crossAxis;  // Misalignment
    float noiseStdDev;
};

simulator.setErrors(errors);
```

### Validation
1. **Accel Bias:** Set known bias, run calibration, verify removed
2. **Gyro Bias:** Same pattern
3. **Mag Hard Iron:** Inject offset, verify compensation
4. **Mag Soft Iron:** Inject scaling, verify compensation

## Build Integration

### CMake Targets
- `helix_simulators` - Library with all simulators
- `helix_integration_tests` - Test executable

### Running Tests
```bash
nix develop
./build.py --host-only -t
# or
./build-arm/helix_integration_tests
```

## Future Enhancements

1. **Level C Fidelity:** Temperature drift, aging, vibration
2. **Multiple Nodes:** Simulate multi-node setup
3. **Fault Injection:** Communication errors, sensor failures
4. **Visualization:** Export to BVH or live plot
