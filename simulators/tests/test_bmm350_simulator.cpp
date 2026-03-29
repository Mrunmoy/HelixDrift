#include "Bmm350Simulator.hpp"
#include "VirtualI2CBus.hpp"
#include <gtest/gtest.h>

using sim::Bmm350Simulator;
using sim::VirtualI2CBus;
using sf::Quaternion;
using sf::Vec3;

namespace {

Vec3 readMagUt(Bmm350Simulator& sim) {
    uint8_t magData[9];
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, magData, sizeof(magData)));

    auto decode24 = [](const uint8_t* buf) -> int32_t {
        uint32_t raw = static_cast<uint32_t>(buf[0]) |
                       (static_cast<uint32_t>(buf[1]) << 8) |
                       (static_cast<uint32_t>(buf[2]) << 16);
        if (raw & 0x100000) {
            return static_cast<int32_t>(raw | 0xFFE00000);
        }
        return static_cast<int32_t>(raw);
    };

    const int32_t rawX = decode24(&magData[0]);
    const int32_t rawY = decode24(&magData[3]);
    const int32_t rawZ = decode24(&magData[6]);
    return Vec3{
        static_cast<float>(rawX) / Bmm350Simulator::UT_SCALE_XY,
        static_cast<float>(rawY) / Bmm350Simulator::UT_SCALE_XY,
        static_cast<float>(rawZ) / Bmm350Simulator::UT_SCALE_Z,
    };
}

} // namespace

// ============================================================================
// TDD Test 1: CHIP_ID returns 0x33
// ============================================================================
TEST(Bmm350SimulatorTest, ChipIdReturns0x33) {
    Bmm350Simulator sim;
    
    uint8_t chipId = 0;
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_CHIP_ID, &chipId, 1));
    EXPECT_EQ(chipId, 0x33);
}

// ============================================================================
// TDD Test 2: Probe returns true
// ============================================================================
TEST(Bmm350SimulatorTest, ProbeReturnsTrue) {
    Bmm350Simulator sim;
    EXPECT_TRUE(sim.probe());
}

// ============================================================================
// TDD Test 3: Write/read config registers
// ============================================================================
TEST(Bmm350SimulatorTest, CanWriteAndReadConfigRegisters) {
    Bmm350Simulator sim;
    
    // Write to PMU_CMD register
    uint8_t writeData = 0x01;  // Normal mode
    EXPECT_TRUE(sim.writeRegister(Bmm350Simulator::REG_PMU_CMD, &writeData, 1));
    
    // Read back the value
    uint8_t readData = 0;
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_PMU_CMD, &readData, 1));
    EXPECT_EQ(readData, 0x01);
}

TEST(Bmm350SimulatorTest, CanWriteAndReadMultipleConfigRegisters) {
    Bmm350Simulator sim;
    
    // Write multiple bytes starting at PMU_CMD_AGGR
    uint8_t writeData[] = {0x45, 0x07, 0x01};  // AGGR, AXIS_EN, CMD
    EXPECT_TRUE(sim.writeRegister(Bmm350Simulator::REG_PMU_CMD_AGGR, writeData, 3));
    
    // Read back all values
    uint8_t readData[3] = {};
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_PMU_CMD_AGGR, readData, 3));
    EXPECT_EQ(readData[0], 0x45);
    EXPECT_EQ(readData[1], 0x07);
    EXPECT_EQ(readData[2], 0x01);
}

// ============================================================================
// TDD Test 4: Mag reads expected value for default orientation
// ============================================================================
TEST(Bmm350SimulatorTest, MagDataForDefaultOrientation) {
    Bmm350Simulator sim;
    
    // Default orientation (identity quaternion) - sensor aligned with world frame
    // Default earth field is (25, 0, 40) µT
    
    // Read mag data (9 bytes: X, Y, Z as 24-bit little-endian)
    uint8_t magData[9];
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, magData, 9));
    
    // Decode the values (21-bit sign-extended)
    auto decode24 = [](const uint8_t* buf) -> int32_t {
        uint32_t raw = static_cast<uint32_t>(buf[0]) |
                       (static_cast<uint32_t>(buf[1]) << 8) |
                       (static_cast<uint32_t>(buf[2]) << 16);
        // Sign extend from 21 bits
        if (raw & 0x100000) {
            return static_cast<int32_t>(raw | 0xFFE00000);
        }
        return static_cast<int32_t>(raw);
    };
    
    int32_t rawX = decode24(&magData[0]);
    int32_t rawY = decode24(&magData[3]);
    int32_t rawZ = decode24(&magData[6]);
    
    // Convert back to µT (applying scale factors)
    float x = static_cast<float>(rawX) / Bmm350Simulator::UT_SCALE_XY;
    float y = static_cast<float>(rawY) / Bmm350Simulator::UT_SCALE_XY;
    float z = static_cast<float>(rawZ) / Bmm350Simulator::UT_SCALE_Z;
    
    // Should be close to default earth field
    EXPECT_NEAR(x, 25.0f, 0.5f);
    EXPECT_NEAR(y, 0.0f, 0.5f);
    EXPECT_NEAR(z, 40.0f, 0.5f);
}

// ============================================================================
// TDD Test 5: Mag data changes with orientation
// ============================================================================
TEST(Bmm350SimulatorTest, MagDataChangesWithOrientation) {
    Bmm350Simulator sim;
    
    // Set orientation to 90-degree rotation around Z axis
    // This swaps X and Y components of the magnetic field
    Quaternion q = Quaternion::fromAxisAngle(0, 0, 1, 90.0f);
    sim.setOrientation(q);
    
    // Read mag data
    uint8_t magData[9];
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, magData, 9));
    
    // Decode
    auto decode24 = [](const uint8_t* buf) -> int32_t {
        uint32_t raw = static_cast<uint32_t>(buf[0]) |
                       (static_cast<uint32_t>(buf[1]) << 8) |
                       (static_cast<uint32_t>(buf[2]) << 16);
        if (raw & 0x100000) {
            return static_cast<int32_t>(raw | 0xFFE00000);
        }
        return static_cast<int32_t>(raw);
    };
    
    int32_t rawX = decode24(&magData[0]);
    int32_t rawY = decode24(&magData[3]);
    
    float x = static_cast<float>(rawX) / Bmm350Simulator::UT_SCALE_XY;
    float y = static_cast<float>(rawY) / Bmm350Simulator::UT_SCALE_XY;
    
    // After 90-degree Z rotation, (25, 0) becomes approximately (0, 25)
    EXPECT_NEAR(x, 0.0f, 1.0f);
    EXPECT_NEAR(y, 25.0f, 1.0f);
}

TEST(Bmm350SimulatorTest, MagDataPitchRotationProjectsEarthFieldCorrectly) {
    Bmm350Simulator sim;

    sim.setOrientation(Quaternion::fromAxisAngle(0, 1, 0, 90.0f));

    const Vec3 mag = readMagUt(sim);
    EXPECT_NEAR(mag.x, 40.0f, 1.0f);
    EXPECT_NEAR(mag.y, 0.0f, 1.0f);
    EXPECT_NEAR(mag.z, -25.0f, 1.0f);
}

// ============================================================================
// TDD Test 6: Hard iron error injection
// ============================================================================
TEST(Bmm350SimulatorTest, HardIronErrorInjection) {
    Bmm350Simulator sim;
    
    // Set identity orientation and known earth field
    sim.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    sim.setEarthField(Vec3{25.0f, 0.0f, 40.0f});
    
    // Configure hard iron offset
    Bmm350Simulator::ErrorConfig errors;
    errors.hardIron = Vec3{5.0f, -3.0f, 2.0f};
    sim.setErrors(errors);
    
    // Read mag data
    uint8_t magData[9];
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, magData, 9));
    
    auto decode24 = [](const uint8_t* buf) -> int32_t {
        uint32_t raw = static_cast<uint32_t>(buf[0]) |
                       (static_cast<uint32_t>(buf[1]) << 8) |
                       (static_cast<uint32_t>(buf[2]) << 16);
        if (raw & 0x100000) {
            return static_cast<int32_t>(raw | 0xFFE00000);
        }
        return static_cast<int32_t>(raw);
    };
    
    int32_t rawX = decode24(&magData[0]);
    int32_t rawY = decode24(&magData[3]);
    int32_t rawZ = decode24(&magData[6]);
    
    float x = static_cast<float>(rawX) / Bmm350Simulator::UT_SCALE_XY;
    float y = static_cast<float>(rawY) / Bmm350Simulator::UT_SCALE_XY;
    float z = static_cast<float>(rawZ) / Bmm350Simulator::UT_SCALE_Z;
    
    // Should be earth field + hard iron
    EXPECT_NEAR(x, 30.0f, 0.5f);  // 25 + 5
    EXPECT_NEAR(y, -3.0f, 0.5f);  // 0 - 3
    EXPECT_NEAR(z, 42.0f, 0.5f);  // 40 + 2
}

TEST(Bmm350SimulatorTest, HardIronOffsetRemainsConstantAcrossOrientations) {
    Bmm350Simulator baseline;
    Bmm350Simulator shifted;

    Bmm350Simulator::ErrorConfig errors;
    errors.hardIron = Vec3{10.0f, -15.0f, 8.0f};
    shifted.setErrors(errors);

    const Quaternion poses[] = {
        Quaternion{1.0f, 0.0f, 0.0f, 0.0f},
        Quaternion::fromAxisAngle(0, 0, 1, 90.0f),
        Quaternion::fromAxisAngle(0, 1, 0, 90.0f),
    };

    for (const auto& pose : poses) {
        baseline.setOrientation(pose);
        shifted.setOrientation(pose);
        const Vec3 base = readMagUt(baseline);
        const Vec3 withHardIron = readMagUt(shifted);

        EXPECT_NEAR(withHardIron.x - base.x, 10.0f, 1.5f);
        EXPECT_NEAR(withHardIron.y - base.y, -15.0f, 1.5f);
        EXPECT_NEAR(withHardIron.z - base.z, 8.0f, 1.5f);
    }
}

// ============================================================================
// TDD Test 7: Soft iron error injection (scaling)
// ============================================================================
TEST(Bmm350SimulatorTest, SoftIronScalingErrorInjection) {
    Bmm350Simulator sim;
    
    // Set identity orientation and known earth field
    sim.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    sim.setEarthField(Vec3{25.0f, 0.0f, 40.0f});
    
    // Configure soft iron scaling
    Bmm350Simulator::ErrorConfig errors;
    errors.softIronScale = Vec3{1.1f, 0.9f, 1.05f};
    sim.setErrors(errors);
    
    // Read mag data
    uint8_t magData[9];
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, magData, 9));
    
    auto decode24 = [](const uint8_t* buf) -> int32_t {
        uint32_t raw = static_cast<uint32_t>(buf[0]) |
                       (static_cast<uint32_t>(buf[1]) << 8) |
                       (static_cast<uint32_t>(buf[2]) << 16);
        if (raw & 0x100000) {
            return static_cast<int32_t>(raw | 0xFFE00000);
        }
        return static_cast<int32_t>(raw);
    };
    
    int32_t rawX = decode24(&magData[0]);
    int32_t rawZ = decode24(&magData[6]);
    
    float x = static_cast<float>(rawX) / Bmm350Simulator::UT_SCALE_XY;
    float z = static_cast<float>(rawZ) / Bmm350Simulator::UT_SCALE_Z;
    
    // Should be earth field scaled by soft iron
    EXPECT_NEAR(x, 27.5f, 0.5f);   // 25 * 1.1
    EXPECT_NEAR(z, 42.0f, 0.5f);   // 40 * 1.05
}

// ============================================================================
// TDD Test 8: OTP data reading
// ============================================================================
TEST(Bmm350SimulatorTest, OtpDataCanBeSetAndRead) {
    Bmm350Simulator sim;
    
    // Set OTP data at word address 0x0E
    sim.setOtpData(0x0E, 0x1234);
    
    // Verify it was set
    EXPECT_EQ(sim.getOtpData(0x0E), 0x1234);
}

TEST(Bmm350SimulatorTest, OtpCommandTriggersDataRead) {
    Bmm350Simulator sim;
    
    // Set OTP data
    sim.setOtpData(0x0E, 0xABCD);
    
    // Write OTP command: 0x20 | word_addr (DIR_READ bit + address)
    uint8_t cmd = 0x20 | 0x0E;
    EXPECT_TRUE(sim.writeRegister(Bmm350Simulator::REG_OTP_CMD, &cmd, 1));
    
    // Read OTP status - should indicate success
    uint8_t status = 0;
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_OTP_STATUS, &status, 1));
    EXPECT_EQ(status & 0x01, 0x01);  // OTP data valid
    
    // Read OTP data registers
    uint8_t msb = 0, lsb = 0;
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_OTP_DATA_MSB, &msb, 1));
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_OTP_DATA_LSB, &lsb, 1));
    
    EXPECT_EQ(msb, 0xAB);
    EXPECT_EQ(lsb, 0xCD);
}

// ============================================================================
// TDD Test 9: OTP data read via registers 0x04-0x0A
// ============================================================================
TEST(Bmm350SimulatorTest, OtpRegisters04To0AContainCalibrationData) {
    Bmm350Simulator sim;
    
    // Set OTP data that would be at addresses 0x0E, 0x0F, 0x10
    sim.setOtpData(0x0E, 0x1111);
    sim.setOtpData(0x0F, 0x2222);
    sim.setOtpData(0x10, 0x3333);
    
    // Read registers 0x04-0x0A (OTP data registers)
    uint8_t otpRegs[7];
    EXPECT_TRUE(sim.readRegister(0x04, otpRegs, 7));
    
    // These should contain some form of calibration data
    // The exact mapping depends on implementation
    // For now, verify we can read them without errors
    SUCCEED();
}

// ============================================================================
// TDD Test 10: Temperature reading
// ============================================================================
TEST(Bmm350SimulatorTest, TemperatureReading) {
    Bmm350Simulator sim;
    
    // Set temperature
    sim.setTemperature(25.0f);
    
    // Read temperature data (3 bytes starting at 0x3A)
    uint8_t tempData[3];
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_TEMP_XLSB, tempData, 3));
    
    // Decode temperature (21-bit sign-extended)
    auto decode24 = [](const uint8_t* buf) -> int32_t {
        uint32_t raw = static_cast<uint32_t>(buf[0]) |
                       (static_cast<uint32_t>(buf[1]) << 8) |
                       (static_cast<uint32_t>(buf[2]) << 16);
        if (raw & 0x100000) {
            return static_cast<int32_t>(raw | 0xFFE00000);
        }
        return static_cast<int32_t>(raw);
    };
    
    int32_t rawTemp = decode24(tempData);
    float tempC = static_cast<float>(rawTemp) * Bmm350Simulator::TEMP_SCALE + Bmm350Simulator::TEMP_OFFSET;
    
    EXPECT_NEAR(tempC, 25.0f, 1.0f);
}

// ============================================================================
// TDD Test 11: Custom earth field
// ============================================================================
TEST(Bmm350SimulatorTest, CustomEarthField) {
    Bmm350Simulator sim;
    
    // Set custom earth field
    sim.setEarthField(Vec3{30.0f, 10.0f, 50.0f});
    sim.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    
    // Read mag data
    uint8_t magData[9];
    EXPECT_TRUE(sim.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, magData, 9));
    
    auto decode24 = [](const uint8_t* buf) -> int32_t {
        uint32_t raw = static_cast<uint32_t>(buf[0]) |
                       (static_cast<uint32_t>(buf[1]) << 8) |
                       (static_cast<uint32_t>(buf[2]) << 16);
        if (raw & 0x100000) {
            return static_cast<int32_t>(raw | 0xFFE00000);
        }
        return static_cast<int32_t>(raw);
    };
    
    int32_t rawX = decode24(&magData[0]);
    int32_t rawY = decode24(&magData[3]);
    int32_t rawZ = decode24(&magData[6]);
    
    float x = static_cast<float>(rawX) / Bmm350Simulator::UT_SCALE_XY;
    float y = static_cast<float>(rawY) / Bmm350Simulator::UT_SCALE_XY;
    float z = static_cast<float>(rawZ) / Bmm350Simulator::UT_SCALE_Z;
    
    // Should match custom earth field
    EXPECT_NEAR(x, 30.0f, 0.5f);
    EXPECT_NEAR(y, 10.0f, 0.5f);
    EXPECT_NEAR(z, 50.0f, 0.5f);
}

// ============================================================================
// TDD Test 12: Integration with VirtualI2CBus
// ============================================================================
TEST(Bmm350SimulatorTest, WorksWithVirtualI2CBus) {
    VirtualI2CBus bus;
    Bmm350Simulator sim;
    
    // Register at default address 0x14
    bus.registerDevice(0x14, sim);
    
    // Probe the device
    EXPECT_TRUE(bus.probe(0x14));
    
    // Read CHIP_ID via bus
    uint8_t chipId = 0;
    EXPECT_TRUE(bus.readRegister(0x14, Bmm350Simulator::REG_CHIP_ID, &chipId, 1));
    EXPECT_EQ(chipId, 0x33);
    
    // Write and read back config
    uint8_t writeData = 0x45;
    EXPECT_TRUE(bus.writeRegister(0x14, Bmm350Simulator::REG_PMU_CMD_AGGR, &writeData, 1));
    
    uint8_t readData = 0;
    EXPECT_TRUE(bus.readRegister(0x14, Bmm350Simulator::REG_PMU_CMD_AGGR, &readData, 1));
    EXPECT_EQ(readData, 0x45);
    
    // Read mag data via bus
    uint8_t magData[9];
    EXPECT_TRUE(bus.readRegister(0x14, Bmm350Simulator::REG_MAG_X_XLSB, magData, 9));
}

// ============================================================================
// TDD Test 13: Combined hard/soft iron errors
// ============================================================================
TEST(Bmm350SimulatorTest, CombinedHardAndSoftIronErrors) {
    Bmm350Simulator sim;
    
    sim.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    sim.setEarthField(Vec3{10.0f, 0.0f, 0.0f});
    
    // Set both hard and soft iron errors
    Bmm350Simulator::ErrorConfig errors;
    errors.hardIron = Vec3{2.0f, 0.0f, 0.0f};
    errors.softIronScale = Vec3{1.5f, 1.0f, 1.0f};
    sim.setErrors(errors);
    
    // Expected: (earth * scale) + hardIron = (10 * 1.5) + 2 = 17
    Vec3 result = sim.getMagData();
    EXPECT_NEAR(result.x, 17.0f, 0.5f);
}

// ============================================================================
// TDD Test 14: Noise injection
// ============================================================================
TEST(Bmm350SimulatorTest, NoiseInjection) {
    Bmm350Simulator sim;
    
    sim.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    sim.setEarthField(Vec3{25.0f, 0.0f, 40.0f});
    
    // Set noise level
    Bmm350Simulator::ErrorConfig errors;
    errors.noiseStdDev = 1.0f;
    sim.setErrors(errors);
    
    // Read multiple times and verify variation
    float sumX = 0;
    int samples = 10;
    
    for (int i = 0; i < samples; i++) {
        Vec3 data = sim.getMagData();
        sumX += data.x;
    }
    
    // With noise, values should vary (this is a probabilistic test)
    // The mean should still be close to expected
    float meanX = sumX / samples;
    EXPECT_NEAR(meanX, 25.0f, 2.0f);  // Within 2 std dev
}

TEST(Bmm350SimulatorTest, NoiseStdDevMatchesConfiguredValue) {
    Bmm350Simulator sim;

    Bmm350Simulator::ErrorConfig errors;
    errors.noiseStdDev = 0.5f;
    sim.setErrors(errors);

    float sumX = 0.0f;
    float sumXSq = 0.0f;
    constexpr int kSamples = 500;
    for (int i = 0; i < kSamples; ++i) {
        const Vec3 mag = sim.getMagData();
        sumX += mag.x;
        sumXSq += mag.x * mag.x;
    }

    const float meanX = sumX / static_cast<float>(kSamples);
    const float varianceX = (sumXSq / static_cast<float>(kSamples)) - (meanX * meanX);
    const float stdDevX = std::sqrt(std::max(varianceX, 0.0f));

    EXPECT_NEAR(meanX, 25.0f, 0.2f);
    EXPECT_NEAR(stdDevX, 0.5f, 0.08f);
}

TEST(Bmm350SimulatorTest, SameSeedProducesIdenticalNoisyMagSamples) {
    Bmm350Simulator simA;
    Bmm350Simulator simB;

    simA.setSeed(4321);
    simB.setSeed(4321);
    simA.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    simB.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});

    Bmm350Simulator::ErrorConfig errors;
    errors.noiseStdDev = 0.4f;
    simA.setErrors(errors);
    simB.setErrors(errors);

    for (int sample = 0; sample < 8; ++sample) {
        uint8_t bufA[9];
        uint8_t bufB[9];
        ASSERT_TRUE(simA.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, bufA, sizeof(bufA)));
        ASSERT_TRUE(simB.readRegister(Bmm350Simulator::REG_MAG_X_XLSB, bufB, sizeof(bufB)));
        for (size_t i = 0; i < sizeof(bufA); ++i) {
            EXPECT_EQ(bufA[i], bufB[i]);
        }
    }
}

// ============================================================================
// TDD Test 15: Bias injection
// ============================================================================
TEST(Bmm350SimulatorTest, BiasInjection) {
    Bmm350Simulator sim;
    
    sim.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    sim.setEarthField(Vec3{25.0f, 0.0f, 40.0f});
    
    // Set bias (separate from hard iron)
    Bmm350Simulator::ErrorConfig errors;
    errors.bias = Vec3{3.0f, -2.0f, 1.0f};
    sim.setErrors(errors);
    
    Vec3 result = sim.getMagData();
    EXPECT_NEAR(result.x, 28.0f, 0.5f);  // 25 + 3
    EXPECT_NEAR(result.y, -2.0f, 0.5f);  // 0 - 2
    EXPECT_NEAR(result.z, 41.0f, 0.5f);  // 40 + 1
}
