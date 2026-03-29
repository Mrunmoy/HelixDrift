#include "Lps22dfSimulator.hpp"
#include "VirtualI2CBus.hpp"
#include <gtest/gtest.h>
#include <cmath>

using sim::Lps22dfSimulator;
using sim::VirtualI2CBus;

namespace {

float readPressureHpa(VirtualI2CBus& bus, uint8_t addr) {
    uint8_t buf[3];
    EXPECT_TRUE(bus.readRegister(addr, 0x28, buf, 3));
    int32_t raw = static_cast<int32_t>(buf[0]) |
                  (static_cast<int32_t>(buf[1]) << 8) |
                  (static_cast<int32_t>(buf[2]) << 16);
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }
    return static_cast<float>(raw) / 4096.0f;
}

float readTemperatureC(VirtualI2CBus& bus, uint8_t addr) {
    uint8_t buf[2];
    EXPECT_TRUE(bus.readRegister(addr, 0x2B, buf, 2));
    const int16_t raw = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    return static_cast<float>(raw) / 100.0f;
}

} // namespace

class Lps22dfSimulatorTest : public ::testing::Test {
protected:
    VirtualI2CBus bus;
    Lps22dfSimulator sensor;
    
    static constexpr uint8_t LPS22DF_ADDR = 0x5D;
    
    void SetUp() override {
        bus.registerDevice(LPS22DF_ADDR, sensor);
    }
};

// Test WHO_AM_I returns 0xB4
TEST_F(Lps22dfSimulatorTest, WhoAmIReturnsCorrectValue) {
    uint8_t id;
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x0F, &id, 1));
    EXPECT_EQ(id, 0xB4);
}

// Test probe works
TEST_F(Lps22dfSimulatorTest, ProbeReturnsTrue) {
    EXPECT_TRUE(bus.probe(LPS22DF_ADDR));
}

// Test CTRL_REG1 can be written and read back
TEST_F(Lps22dfSimulatorTest, CtrlReg1ReadWrite) {
    uint8_t writeData = 0x38;  // ODR=7, AVG=0
    EXPECT_TRUE(bus.writeRegister(LPS22DF_ADDR, 0x10, &writeData, 1));
    
    uint8_t readData;
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x10, &readData, 1));
    EXPECT_EQ(readData, writeData);
}

// Test CTRL_REG3 can be written and read back (IF_ADD_INC)
TEST_F(Lps22dfSimulatorTest, CtrlReg3ReadWrite) {
    uint8_t writeData = 0x04;  // IF_ADD_INC enabled
    EXPECT_TRUE(bus.writeRegister(LPS22DF_ADDR, 0x12, &writeData, 1));
    
    uint8_t readData;
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x12, &readData, 1));
    EXPECT_EQ(readData, writeData);
}

// Test software reset via CTRL_REG2 clears writable control registers
TEST_F(Lps22dfSimulatorTest, SoftwareResetClearsWritableControlRegisters) {
    uint8_t ctrlReg1 = 0x38;
    uint8_t ctrlReg3 = 0x04;
    uint8_t ctrlReg4 = 0x80;
    EXPECT_TRUE(bus.writeRegister(LPS22DF_ADDR, 0x10, &ctrlReg1, 1));
    EXPECT_TRUE(bus.writeRegister(LPS22DF_ADDR, 0x12, &ctrlReg3, 1));
    EXPECT_TRUE(bus.writeRegister(LPS22DF_ADDR, 0x13, &ctrlReg4, 1));

    uint8_t swReset = 0x04;  // SWRESET bit in CTRL_REG2
    EXPECT_TRUE(bus.writeRegister(LPS22DF_ADDR, 0x11, &swReset, 1));

    uint8_t readCtrl1 = 0xFF;
    uint8_t readCtrl2 = 0xFF;
    uint8_t readCtrl3 = 0xFF;
    uint8_t readCtrl4 = 0xFF;
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x10, &readCtrl1, 1));
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x11, &readCtrl2, 1));
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x12, &readCtrl3, 1));
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x13, &readCtrl4, 1));

    EXPECT_EQ(readCtrl1, 0x00);
    EXPECT_EQ(readCtrl2, 0x00);
    EXPECT_EQ(readCtrl3, 0x00);
    EXPECT_EQ(readCtrl4, 0x00);
}

// Test STATUS register exists and can be read
TEST_F(Lps22dfSimulatorTest, StatusRegisterCanBeRead) {
    uint8_t status;
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x0B, &status, 1));
    // Status bits may vary based on configuration, just verify read succeeds
}

// Test pressure reads expected value at sea level (0m altitude)
TEST_F(Lps22dfSimulatorTest, PressureAtSeaLevel) {
    sensor.setAltitude(0.0f);  // Sea level
    
    uint8_t buf[3];
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x28, buf, 3));
    
    // Convert back from little-endian 24-bit
    int32_t raw = static_cast<int32_t>(buf[0]) |
                  (static_cast<int32_t>(buf[1]) << 8) |
                  (static_cast<int32_t>(buf[2]) << 16);
    // Sign-extend if negative
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }
    
    float hPa = static_cast<float>(raw) / 4096.0f;
    
    // Should be approximately 1013.25 hPa at sea level
    EXPECT_NEAR(hPa, 1013.25f, 0.5f);
}

// Test pressure decreases with altitude
TEST_F(Lps22dfSimulatorTest, PressureDecreasesWithAltitude) {
    sensor.setAltitude(0.0f);
    float hPa0 = readPressureHpa(bus, LPS22DF_ADDR);
    
    // Now set altitude to 1000m
    sensor.setAltitude(1000.0f);
    float hPa1000 = readPressureHpa(bus, LPS22DF_ADDR);
    
    // Pressure should be lower at higher altitude
    EXPECT_LT(hPa1000, hPa0);
    
    // Approximately 120 hPa difference per 1000m (using barometric formula)
    EXPECT_NEAR(hPa0 - hPa1000, 120.0f, 20.0f);
}

// Test temperature reads realistic value
TEST_F(Lps22dfSimulatorTest, TemperatureIsRealistic) {
    float tempC = readTemperatureC(bus, LPS22DF_ADDR);
    
    // Should be a realistic temperature (-40 to +85 is sensor range)
    EXPECT_GT(tempC, -40.0f);
    EXPECT_LT(tempC, 85.0f);
    
    // Default should be around room temperature (20-25°C)
    EXPECT_NEAR(tempC, 22.0f, 10.0f);
}

// Test setPressure overrides calculated value
TEST_F(Lps22dfSimulatorTest, SetPressureOverridesAltitude) {
    sensor.setAltitude(0.0f);
    sensor.setPressure(950.0f);  // Force specific pressure
    float hPa = readPressureHpa(bus, LPS22DF_ADDR);
    EXPECT_NEAR(hPa, 950.0f, 0.1f);
}

// Test pressure bias injection
TEST_F(Lps22dfSimulatorTest, PressureBias) {
    sensor.setAltitude(0.0f);
    sensor.setPressureBias(10.0f);  // Add 10 hPa bias
    float hPa = readPressureHpa(bus, LPS22DF_ADDR);
    EXPECT_NEAR(hPa, 1013.25f + 10.0f, 0.5f);
}

// Test temperature bias injection
TEST_F(Lps22dfSimulatorTest, TemperatureBias) {
    sensor.setTemperatureBias(5.0f);  // Add 5°C bias
    float tempC = readTemperatureC(bus, LPS22DF_ADDR);
    
    // Should be around 22 + 5 = 27°C
    EXPECT_NEAR(tempC, 27.0f, 10.0f);  // Allow some default variation
}

// Test pressure noise can be configured (statistical test)
TEST_F(Lps22dfSimulatorTest, PressureNoiseStdDev) {
    sensor.setAltitude(0.0f);
    sensor.setPressureNoiseStdDev(1.0f);  // 1 hPa std dev
    
    // Read multiple samples
    std::vector<float> samples;
    for (int i = 0; i < 100; i++) {
        uint8_t buf[3];
        bus.readRegister(LPS22DF_ADDR, 0x28, buf, 3);
        int32_t raw = static_cast<int32_t>(buf[0]) |
                      (static_cast<int32_t>(buf[1]) << 8) |
                      (static_cast<int32_t>(buf[2]) << 16);
        if (raw & 0x800000) raw |= 0xFF000000;
        samples.push_back(static_cast<float>(raw) / 4096.0f);
    }
    
    // Calculate standard deviation
    float sum = 0.0f;
    for (float s : samples) sum += s;
    float mean = sum / samples.size();
    
    float variance = 0.0f;
    for (float s : samples) variance += (s - mean) * (s - mean);
    float stddev = std::sqrt(variance / samples.size());
    
    // Std dev should be roughly 1.0 hPa (allow some statistical variation)
    EXPECT_NEAR(stddev, 1.0f, 0.5f);
    
    // Mean should still be close to expected value
    EXPECT_NEAR(mean, 1013.25f, 0.5f);
}

// Test auto-increment read (multiple registers in one transaction)
TEST_F(Lps22dfSimulatorTest, MultiRegisterRead) {
    sensor.setAltitude(0.0f);
    
    // Read both pressure (3 bytes) and temperature (2 bytes) at once
    uint8_t buf[5];
    EXPECT_TRUE(bus.readRegister(LPS22DF_ADDR, 0x28, buf, 5));
    
    // Parse pressure
    int32_t rawP = static_cast<int32_t>(buf[0]) |
                   (static_cast<int32_t>(buf[1]) << 8) |
                   (static_cast<int32_t>(buf[2]) << 16);
    if (rawP & 0x800000) rawP |= 0xFF000000;
    float hPa = static_cast<float>(rawP) / 4096.0f;
    
    // Parse temperature
    int16_t rawT = static_cast<int16_t>(buf[3] | (buf[4] << 8));
    float tempC = static_cast<float>(rawT) / 100.0f;
    
    EXPECT_NEAR(hPa, 1013.25f, 0.5f);
    EXPECT_GT(tempC, -40.0f);
    EXPECT_LT(tempC, 85.0f);
}

// Test sea level pressure can be configured
TEST_F(Lps22dfSimulatorTest, SetBasePressure) {
    sensor.setBasePressure(1000.0f);  // Set base to 1000 hPa
    sensor.setAltitude(0.0f);
    float hPa = readPressureHpa(bus, LPS22DF_ADDR);
    EXPECT_NEAR(hPa, 1000.0f, 0.5f);
}

// Test temperature can be set directly
TEST_F(Lps22dfSimulatorTest, SetTemperature) {
    sensor.setTemperature(30.0f);  // Set to 30°C
    float tempC = readTemperatureC(bus, LPS22DF_ADDR);
    EXPECT_NEAR(tempC, 30.0f, 0.1f);
}

TEST_F(Lps22dfSimulatorTest, PressureAtBelowSeaLevelMatchesBarometricExpectation) {
    sensor.setAltitude(-100.0f);

    const float hPa = readPressureHpa(bus, LPS22DF_ADDR);
    EXPECT_NEAR(hPa, 1025.4f, 1.0f);
}

TEST_F(Lps22dfSimulatorTest, ColdTemperatureReadbackMatchesConfiguredValue) {
    sensor.setTemperature(-10.0f);

    const float tempC = readTemperatureC(bus, LPS22DF_ADDR);
    EXPECT_NEAR(tempC, -10.0f, 0.5f);
}

TEST_F(Lps22dfSimulatorTest, HotTemperatureReadbackMatchesConfiguredValue) {
    sensor.setTemperature(60.0f);

    const float tempC = readTemperatureC(bus, LPS22DF_ADDR);
    EXPECT_NEAR(tempC, 60.0f, 0.5f);
}

TEST(Lps22dfSimulatorDeterminismTest, SameSeedProducesIdenticalNoisyPressureSamples) {
    Lps22dfSimulator sensorA;
    Lps22dfSimulator sensorB;

    sensorA.setSeed(9876);
    sensorB.setSeed(9876);
    sensorA.setAltitude(123.0f);
    sensorB.setAltitude(123.0f);
    sensorA.setPressureNoiseStdDev(0.7f);
    sensorB.setPressureNoiseStdDev(0.7f);

    for (int sample = 0; sample < 8; ++sample) {
        uint8_t bufA[3];
        uint8_t bufB[3];
        ASSERT_TRUE(sensorA.readRegister(0x28, bufA, sizeof(bufA)));
        ASSERT_TRUE(sensorB.readRegister(0x28, bufB, sizeof(bufB)));
        for (size_t i = 0; i < sizeof(bufA); ++i) {
            EXPECT_EQ(bufA[i], bufB[i]);
        }
    }
}
