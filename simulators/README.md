# Sensor Simulators

Integration testing framework for HelixDrift sensor stack.

## Overview

This directory contains virtual I2C devices and motion simulation for testing the sensor fusion pipeline off-target.

## Architecture

```
simulators/
├── i2c/              # Virtual I2C bus implementation
├── sensors/          # Sensor simulators (LSM6DSO, BMM350, LPS22DF)
├── gimbal/           # Virtual gimbal and motion control
├── storage/          # I2C EEPROM simulation
├── tests/            # Integration tests
└── motion_profiles/  # Pre-defined motion sequences (JSON)
```

## Quick Start

```bash
# Enter nix environment
nix develop

# Build and run integration tests
./build.py --host-only -t

# Run only integration tests
./build-arm/helix_integration_tests
```

## Components

### VirtualI2CBus
Routes I2C transactions to simulated sensor devices. Implements `sf::II2CBus` interface so real sensor drivers can communicate with virtual devices.

### Sensor Simulators
Each sensor maintains internal register state and returns realistic data based on:
- Current gimbal orientation
- Configured noise/bias/scale errors
- Temperature effects

### VirtualGimbal
Controls the "physical" orientation of the sensor block:
- Programmatic API: `setOrientation(quaternion)`, `rotate(rate)`
- Motion scripts: Load JSON sequences
- Physics: Realistic rotation dynamics

## Testing Strategy

1. **Unit Tests (in `tests/`)**: Test individual components in isolation
2. **Integration Tests (in `simulators/tests/`)**: Test sensor + fusion pipeline together

## Current Status

See [TASKS.md](TASKS.md) for current work items.
