#include <gtest/gtest.h>
#include "Lsm6dsoSimulator.hpp"
#include "VirtualI2CBus.hpp"
#include "Quaternion.hpp"
#include "Vec3.hpp"

using sim::Lsm6dsoSimulator;
using sim::VirtualI2CBus;
using sf::Quaternion;
using sf::Vec3;

namespace {

Vec3 readAccelG(VirtualI2CBus& bus, uint8_t addr) {
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(addr, Lsm6dsoSimulator::REG_OUTX_L_A, buf, sizeof(buf)));
    const int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    const int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    const int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    constexpr float kAccelSens = 0.061f;
    return Vec3{
        rawX * kAccelSens * 0.001f,
        rawY * kAccelSens * 0.001f,
        rawZ * kAccelSens * 0.001f,
    };
}

Vec3 readGyroDps(VirtualI2CBus& bus, uint8_t addr) {
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(addr, Lsm6dsoSimulator::REG_OUTX_L_G, buf, sizeof(buf)));
    const int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    const int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    const int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    constexpr float kGyroSens = 8.75f;
    return Vec3{
        rawX * kGyroSens * 0.001f,
        rawY * kGyroSens * 0.001f,
        rawZ * kGyroSens * 0.001f,
    };
}

float accelNorm(const Vec3& accel) {
    return std::sqrt(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
}

} // namespace

class Lsm6dsoSimulatorTest : public ::testing::Test {
protected:
    VirtualI2CBus bus;
    Lsm6dsoSimulator imu;
    static constexpr uint8_t IMU_ADDR = 0x6A;

    void SetUp() override {
        bus.registerDevice(IMU_ADDR, imu);
    }
};

// Test 1: WHO_AM_I returns 0x6C
TEST_F(Lsm6dsoSimulatorTest, WhoAmI_ReturnsCorrectValue) {
    uint8_t whoami;
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x0F, &whoami, 1));
    EXPECT_EQ(whoami, 0x6C);
}

// Test 2: Probe works
TEST_F(Lsm6dsoSimulatorTest, Probe_Succeeds) {
    EXPECT_TRUE(bus.probe(IMU_ADDR));
}

// Test 3: Write and read CTRL1_XL (accel config)
TEST_F(Lsm6dsoSimulatorTest, WriteRead_Ctrl1Xl) {
    uint8_t config = 0x44; // ODR=104Hz, FS=4g
    EXPECT_TRUE(bus.writeRegister(IMU_ADDR, 0x10, &config, 1));
    
    uint8_t readback;
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x10, &readback, 1));
    EXPECT_EQ(readback, config);
}

// Test 4: Write and read CTRL2_G (gyro config)
TEST_F(Lsm6dsoSimulatorTest, WriteRead_Ctrl2G) {
    uint8_t config = 0x48; // ODR=104Hz, FS=500dps
    EXPECT_TRUE(bus.writeRegister(IMU_ADDR, 0x11, &config, 1));
    
    uint8_t readback;
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x11, &readback, 1));
    EXPECT_EQ(readback, config);
}

// Test 5: Write and read CTRL3_C (control)
TEST_F(Lsm6dsoSimulatorTest, WriteRead_Ctrl3C) {
    uint8_t config = 0x44; // BDU + IF_INC
    EXPECT_TRUE(bus.writeRegister(IMU_ADDR, 0x12, &config, 1));
    
    uint8_t readback;
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x12, &readback, 1));
    EXPECT_EQ(readback, config);
}

// Test 6: Accelerometer reads expected value for flat orientation (gravity down)
// When flat, gravity [0,0,1g] in world frame should read [0,0,1g] in sensor frame
TEST_F(Lsm6dsoSimulatorTest, Accel_FlatOrientation_ReadsGravity) {
    // Set flat orientation (identity quaternion - no rotation)
    imu.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    
    // Read accel data (6 bytes: XL, XH, YL, YH, ZL, ZH)
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf, 6));
    
    // Parse little-endian values
    int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    
    // Convert to g (using 2g range: 0.061 mg/LSB = 0.000061 g/LSB)
    // But wait - the simulator should output in the same format as real sensor
    // At 2g range with 0.061 mg/LSB, 1g = 1/0.000061 ≈ 16384 LSB
    float accelSens = 0.061f; // mg/LSB
    float x = rawX * accelSens * 0.001f;
    float y = rawY * accelSens * 0.001f;
    float z = rawZ * accelSens * 0.001f;
    
    // For flat orientation, we expect approximately [0, 0, 1g]
    EXPECT_NEAR(x, 0.0f, 0.01f);
    EXPECT_NEAR(y, 0.0f, 0.01f);
    EXPECT_NEAR(z, 1.0f, 0.01f);
}

// Test 7: Accelerometer reads inverted gravity when flipped 180 degrees
TEST_F(Lsm6dsoSimulatorTest, Accel_FlippedOrientation_ReadsInvertedGravity) {
    // Rotate 180 degrees around X axis (flip upside down)
    Quaternion q = Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 180.0f);
    imu.setOrientation(q);
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf, 6));
    
    int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    
    float accelSens = 0.061f; // mg/LSB
    float x = rawX * accelSens * 0.001f;
    float y = rawY * accelSens * 0.001f;
    float z = rawZ * accelSens * 0.001f;
    
    // When flipped, gravity should point in -Z direction
    EXPECT_NEAR(x, 0.0f, 0.01f);
    EXPECT_NEAR(y, 0.0f, 0.01f);
    EXPECT_NEAR(z, -1.0f, 0.01f);
}

// Test 8: Accelerometer reads gravity in X when rolled 90 degrees
TEST_F(Lsm6dsoSimulatorTest, Accel_RolledOrientation_ReadsGravityInX) {
    // Roll 90 degrees (rotate around X axis)
    Quaternion q = Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 90.0f);
    imu.setOrientation(q);
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf, 6));
    
    int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    
    float accelSens = 0.061f; // mg/LSB
    float x = rawX * accelSens * 0.001f;
    float y = rawY * accelSens * 0.001f;
    float z = rawZ * accelSens * 0.001f;
    
    // When rolled 90 degrees, gravity should point in -Y direction
    // (positive roll around X rotates Y toward Z, so world Z points to sensor -Y)
    EXPECT_NEAR(x, 0.0f, 0.01f);
    EXPECT_NEAR(y, -1.0f, 0.01f);
    EXPECT_NEAR(z, 0.0f, 0.05f); // Small tolerance for Z
}

// Test 9: Gyro reads expected value for set rotation rate
TEST_F(Lsm6dsoSimulatorTest, Gyro_SetRotationRate_ReadsCorrectValue) {
    // Set rotation rate of 1.0 rad/s around Z axis
    imu.setRotationRate(0.0f, 0.0f, 1.0f);
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x22, buf, 6));
    
    int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    
    float gyroSens = 8.75f; // mdps/LSB for 250 dps range
    float x = rawX * gyroSens * 0.001f; // dps
    float y = rawY * gyroSens * 0.001f;
    float z = rawZ * gyroSens * 0.001f;
    
    // Convert rad/s to dps: 1 rad/s = 180/pi ≈ 57.2958 dps
    float expectedZ_dps = 1.0f * 57.2958f;
    
    EXPECT_NEAR(x, 0.0f, 0.5f);
    EXPECT_NEAR(y, 0.0f, 0.5f);
    EXPECT_NEAR(z, expectedZ_dps, 0.5f);
}

// Test 10: Gyro reads rotation rate around X axis
TEST_F(Lsm6dsoSimulatorTest, Gyro_RotationRateX_ReadsCorrectValue) {
    // Set rotation rate of 2.0 rad/s around X axis
    imu.setRotationRate(2.0f, 0.0f, 0.0f);
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x22, buf, 6));
    
    int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    
    float gyroSens = 8.75f; // mdps/LSB
    float x = rawX * gyroSens * 0.001f;
    float y = rawY * gyroSens * 0.001f;
    float z = rawZ * gyroSens * 0.001f;
    
    float expectedX_dps = 2.0f * 57.2958f;
    
    EXPECT_NEAR(x, expectedX_dps, 0.5f);
    EXPECT_NEAR(y, 0.0f, 0.5f);
    EXPECT_NEAR(z, 0.0f, 0.5f);
}

// Test 11: Temperature sensor returns reasonable value
TEST_F(Lsm6dsoSimulatorTest, Temperature_ReturnsReasonableValue) {
    uint8_t buf[2];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x20, buf, 2));
    
    int16_t rawTemp = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    float tempC = static_cast<float>(rawTemp) / 256.0f + 25.0f;
    
    // Should be around room temperature (20-30C)
    EXPECT_GT(tempC, 15.0f);
    EXPECT_LT(tempC, 40.0f);
}

TEST_F(Lsm6dsoSimulatorTest, Gyro_StationaryReadsZeroOnAllAxes) {
    imu.setRotationRate(0.0f, 0.0f, 0.0f);

    const Vec3 gyro = readGyroDps(bus, IMU_ADDR);
    EXPECT_NEAR(gyro.x, 0.0f, 0.5f);
    EXPECT_NEAR(gyro.y, 0.0f, 0.5f);
    EXPECT_NEAR(gyro.z, 0.0f, 0.5f);
}

TEST_F(Lsm6dsoSimulatorTest, Gyro_MultiAxisRotationReadsExpectedValues) {
    imu.setRotationRate(1.0f, 0.5f, -0.3f);

    const Vec3 gyro = readGyroDps(bus, IMU_ADDR);
    EXPECT_NEAR(gyro.x, 57.2958f, 0.5f);
    EXPECT_NEAR(gyro.y, 28.6479f, 0.5f);
    EXPECT_NEAR(gyro.z, -17.1887f, 0.5f);
}

TEST_F(Lsm6dsoSimulatorTest, AccelMagnitudeStaysNearOneGAcrossKnownOrientations) {
    const Quaternion poses[] = {
        Quaternion{1.0f, 0.0f, 0.0f, 0.0f},
        Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 180.0f),
        Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 90.0f),
        Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f),
        Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, -90.0f),
    };

    for (const auto& pose : poses) {
        imu.setOrientation(pose);
        const Vec3 accel = readAccelG(bus, IMU_ADDR);
        EXPECT_NEAR(accelNorm(accel), 1.0f, 0.02f);
    }
}

// Test 12: Bias injection works for accelerometer
TEST_F(Lsm6dsoSimulatorTest, AccelBias_InjectedCorrectly) {
    // Set bias of 0.1g in X direction
    imu.setAccelBias(Vec3{0.1f, 0.0f, 0.0f});
    imu.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf, 6));
    
    int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    
    float accelSens = 0.061f; // mg/LSB
    float x = rawX * accelSens * 0.001f;
    
    // Should read approximately 0.1g bias + 0g gravity component
    EXPECT_NEAR(x, 0.1f, 0.02f);
}

// Test 13: Bias injection works for gyroscope
TEST_F(Lsm6dsoSimulatorTest, GyroBias_InjectedCorrectly) {
    // Set bias of 0.5 rad/s in Y direction
    imu.setGyroBias(Vec3{0.0f, 0.5f, 0.0f});
    imu.setRotationRate(0.0f, 0.0f, 0.0f); // No actual rotation
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x22, buf, 6));
    
    int16_t rawY = static_cast<int16_t>(buf[2] | (buf[3] << 8));
    
    float gyroSens = 8.75f; // mdps/LSB
    float y = rawY * gyroSens * 0.001f;
    
    // Should read the bias in dps: 0.5 rad/s * 57.2958 dps/rad
    float expectedBias_dps = 0.5f * 57.2958f;
    EXPECT_NEAR(y, expectedBias_dps, 0.5f);
}

// Test 14: Scale factor works for accelerometer
TEST_F(Lsm6dsoSimulatorTest, AccelScale_AppliedCorrectly) {
    // Set scale of 1.1 in Z (10% gain error)
    imu.setAccelScale(Vec3{1.0f, 1.0f, 1.1f});
    imu.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf, 6));
    
    int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
    
    float accelSens = 0.061f; // mg/LSB
    float z = rawZ * accelSens * 0.001f;
    
    // Should read approximately 1.1g (10% scale error on 1g)
    EXPECT_NEAR(z, 1.1f, 0.02f);
}

// Test 15: Scale factor works for gyroscope
TEST_F(Lsm6dsoSimulatorTest, GyroScale_AppliedCorrectly) {
    // Set scale of 0.95 in X (5% gain error)
    imu.setGyroScale(Vec3{0.95f, 1.0f, 1.0f});
    imu.setRotationRate(1.0f, 0.0f, 0.0f);
    
    uint8_t buf[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x22, buf, 6));
    
    int16_t rawX = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    
    float gyroSens = 8.75f; // mdps/LSB
    float x = rawX * gyroSens * 0.001f;
    
    // Should read approximately 95% of expected value
    float expectedX_dps = 1.0f * 57.2958f * 0.95f;
    EXPECT_NEAR(x, expectedX_dps, 0.5f);
}

TEST_F(Lsm6dsoSimulatorTest, AccelRawCountsTrackConfiguredFullScale) {
    imu.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});

    uint8_t accel2gCfg = 0x00;  // FS_XL = 0 => 2g
    ASSERT_TRUE(bus.writeRegister(IMU_ADDR, Lsm6dsoSimulator::REG_CTRL1_XL, &accel2gCfg, 1));

    uint8_t buf2g[6];
    ASSERT_TRUE(bus.readRegister(IMU_ADDR, Lsm6dsoSimulator::REG_OUTX_L_A, buf2g, sizeof(buf2g)));
    int16_t rawZ2g = static_cast<int16_t>(buf2g[4] | (buf2g[5] << 8));

    uint8_t accel8gCfg = 0x0C;  // FS_XL = 3 => 8g
    ASSERT_TRUE(bus.writeRegister(IMU_ADDR, Lsm6dsoSimulator::REG_CTRL1_XL, &accel8gCfg, 1));

    uint8_t buf8g[6];
    ASSERT_TRUE(bus.readRegister(IMU_ADDR, Lsm6dsoSimulator::REG_OUTX_L_A, buf8g, sizeof(buf8g)));
    int16_t rawZ8g = static_cast<int16_t>(buf8g[4] | (buf8g[5] << 8));

    EXPECT_GT(rawZ2g, 0);
    EXPECT_GT(rawZ8g, 0);
    EXPECT_NEAR(static_cast<float>(rawZ2g) / static_cast<float>(rawZ8g), 4.0f, 0.1f);
}

TEST_F(Lsm6dsoSimulatorTest, GyroRawCountsTrackConfiguredFullScale) {
    imu.setRotationRate(0.0f, 0.0f, 1.0f);

    uint8_t gyro250dpsCfg = 0x00;  // FS_G = 0 => 250 dps
    ASSERT_TRUE(bus.writeRegister(IMU_ADDR, Lsm6dsoSimulator::REG_CTRL2_G, &gyro250dpsCfg, 1));

    uint8_t buf250[6];
    ASSERT_TRUE(bus.readRegister(IMU_ADDR, Lsm6dsoSimulator::REG_OUTX_L_G, buf250, sizeof(buf250)));
    int16_t rawZ250 = static_cast<int16_t>(buf250[4] | (buf250[5] << 8));

    uint8_t gyro1000dpsCfg = 0x08;  // FS_G = 4 => 1000 dps
    ASSERT_TRUE(bus.writeRegister(IMU_ADDR, Lsm6dsoSimulator::REG_CTRL2_G, &gyro1000dpsCfg, 1));

    uint8_t buf1000[6];
    ASSERT_TRUE(bus.readRegister(IMU_ADDR, Lsm6dsoSimulator::REG_OUTX_L_G, buf1000, sizeof(buf1000)));
    int16_t rawZ1000 = static_cast<int16_t>(buf1000[4] | (buf1000[5] << 8));

    EXPECT_GT(rawZ250, 0);
    EXPECT_GT(rawZ1000, 0);
    EXPECT_NEAR(static_cast<float>(rawZ250) / static_cast<float>(rawZ1000), 4.0f, 0.1f);
}

// Test 16: Noise injection adds variation (statistical test)
TEST_F(Lsm6dsoSimulatorTest, Noise_AddsVariation) {
    imu.setAccelNoiseStdDev(0.01f); // 0.01g noise
    imu.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    
    // Read multiple times and check variation
    float sumZ = 0.0f;
    float sumZSq = 0.0f;
    const int n = 100;
    
    for (int i = 0; i < n; i++) {
        uint8_t buf[6];
        EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf, 6));
        int16_t rawZ = static_cast<int16_t>(buf[4] | (buf[5] << 8));
        float accelSens = 0.061f;
        float z = rawZ * accelSens * 0.001f;
        sumZ += z;
        sumZSq += z * z;
    }
    
    float meanZ = sumZ / n;
    float varianceZ = (sumZSq / n) - (meanZ * meanZ);
    float stdDevZ = std::sqrt(varianceZ);
    
    // Mean should be close to 1g
    EXPECT_NEAR(meanZ, 1.0f, 0.01f);
    
    // Standard deviation should be close to injected noise (with tolerance)
    // For 0.01g noise, we expect std dev around 0.01g
    EXPECT_GT(stdDevZ, 0.005f); // Should have some variation
    EXPECT_LT(stdDevZ, 0.02f);  // But not too much
}

TEST_F(Lsm6dsoSimulatorTest, GyroNoiseStdDevMatchesConfiguredValue) {
    imu.setGyroNoiseStdDev(0.001f);
    imu.setRotationRate(0.0f, 0.0f, 0.0f);

    float sumZ = 0.0f;
    float sumZSq = 0.0f;
    constexpr int kSamples = 500;
    for (int i = 0; i < kSamples; ++i) {
        const Vec3 gyro = readGyroDps(bus, IMU_ADDR);
        const float zRadPerSec = gyro.z * (3.14159265358979323846f / 180.0f);
        sumZ += zRadPerSec;
        sumZSq += zRadPerSec * zRadPerSec;
    }

    const float meanZ = sumZ / static_cast<float>(kSamples);
    const float varianceZ = (sumZSq / static_cast<float>(kSamples)) - (meanZ * meanZ);
    const float stdDevZ = std::sqrt(std::max(varianceZ, 0.0f));

    EXPECT_NEAR(meanZ, 0.0f, 0.0003f);
    EXPECT_NEAR(stdDevZ, 0.001f, 0.0003f);
}

TEST(Lsm6dsoSimulatorDeterminismTest, SameSeedProducesIdenticalNoisyAccelSamples) {
    Lsm6dsoSimulator imuA;
    Lsm6dsoSimulator imuB;

    imuA.setSeed(1234);
    imuB.setSeed(1234);

    imuA.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    imuB.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    imuA.setAccelNoiseStdDev(0.02f);
    imuB.setAccelNoiseStdDev(0.02f);

    for (int sample = 0; sample < 8; ++sample) {
        uint8_t bufA[6];
        uint8_t bufB[6];
        ASSERT_TRUE(imuA.readRegister(Lsm6dsoSimulator::REG_OUTX_L_A, bufA, sizeof(bufA)));
        ASSERT_TRUE(imuB.readRegister(Lsm6dsoSimulator::REG_OUTX_L_A, bufB, sizeof(bufB)));
        for (size_t i = 0; i < sizeof(bufA); ++i) {
            EXPECT_EQ(bufA[i], bufB[i]);
        }
    }
}

// Test 17: Bulk read from OUT_TEMP_L gets temp + gyro + accel
TEST_F(Lsm6dsoSimulatorTest, BulkRead_AllData) {
    imu.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    imu.setRotationRate(0.0f, 0.0f, 1.0f);
    
    // Read 14 bytes starting at OUT_TEMP_L (0x20)
    // [0:1] temp, [2:7] gyro XYZ, [8:13] accel XYZ
    uint8_t buf[14];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x20, buf, 14));
    
    // Parse temperature
    int16_t rawTemp = static_cast<int16_t>(buf[0] | (buf[1] << 8));
    float tempC = static_cast<float>(rawTemp) / 256.0f + 25.0f;
    EXPECT_GT(tempC, 15.0f);
    EXPECT_LT(tempC, 40.0f);
    
    // Parse gyro Z
    int16_t rawGyroZ = static_cast<int16_t>(buf[6] | (buf[7] << 8));
    float gyroSens = 8.75f;
    float gyroZ = rawGyroZ * gyroSens * 0.001f;
    float expectedGyroZ_dps = 1.0f * 57.2958f;
    EXPECT_NEAR(gyroZ, expectedGyroZ_dps, 0.5f);
    
    // Parse accel Z (bytes 12-13)
    int16_t rawAccelZ = static_cast<int16_t>(buf[12] | (buf[13] << 8));
    float accelSens = 0.061f;
    float accelZ = rawAccelZ * accelSens * 0.001f;
    EXPECT_NEAR(accelZ, 1.0f, 0.01f);
}

// Test 18: Multiple sequential reads return consistent values (no noise)
TEST_F(Lsm6dsoSimulatorTest, SequentialReads_ConsistentWithoutNoise) {
    imu.setAccelNoiseStdDev(0.0f);
    imu.setGyroNoiseStdDev(0.0f);
    imu.setOrientation(Quaternion{1.0f, 0.0f, 0.0f, 0.0f});
    imu.setRotationRate(0.5f, 0.3f, 0.2f);
    
    // Read twice
    uint8_t buf1[6], buf2[6];
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf1, 6));
    EXPECT_TRUE(bus.readRegister(IMU_ADDR, 0x28, buf2, 6));
    
    // Should be identical (no noise)
    EXPECT_EQ(buf1[0], buf2[0]);
    EXPECT_EQ(buf1[1], buf2[1]);
    EXPECT_EQ(buf1[2], buf2[2]);
    EXPECT_EQ(buf1[3], buf2[3]);
    EXPECT_EQ(buf1[4], buf2[4]);
    EXPECT_EQ(buf1[5], buf2[5]);
}
