#include "VirtualSensorAssembly.hpp"

#include <gtest/gtest.h>
#include <cmath>

using namespace sim;
using namespace sf;

TEST(VirtualSensorAssemblyTest, RegistersDevicesOnExpectedBuses) {
    VirtualSensorAssembly assembly;

    EXPECT_TRUE(assembly.imuBus().probe(0x6A));
    EXPECT_TRUE(assembly.auxBus().probe(0x14));
    EXPECT_TRUE(assembly.auxBus().probe(0x5D));
}

TEST(VirtualSensorAssemblyTest, InitAllInitializesThreeSensorAssembly) {
    VirtualSensorAssembly assembly;

    EXPECT_TRUE(assembly.initAll());
}

TEST(VirtualSensorAssemblyTest, GimbalSyncPropagatesPoseToAllSensors) {
    VirtualSensorAssembly assembly;
    ASSERT_TRUE(assembly.initAll());

    assembly.gimbal().setRotationRate({0.0f, 0.0f, 1.0f});
    assembly.gimbal().update(0.1f);
    assembly.gimbal().syncToSensors();

    AccelData accel;
    GyroData gyro;
    MagData mag;
    float pressureHpa = 0.0f;

    EXPECT_TRUE(assembly.imuDriver().readAccel(accel));
    EXPECT_TRUE(assembly.imuDriver().readGyro(gyro));
    EXPECT_TRUE(assembly.magDriver().readMag(mag));
    EXPECT_TRUE(assembly.baroDriver().readPressure(pressureHpa));

    EXPECT_TRUE(std::isfinite(accel.x));
    EXPECT_TRUE(std::isfinite(accel.y));
    EXPECT_TRUE(std::isfinite(accel.z));
    EXPECT_GT(gyro.z, 50.0f);
    EXPECT_TRUE(std::isfinite(mag.x));
    EXPECT_TRUE(std::isfinite(mag.y));
    EXPECT_TRUE(std::isfinite(mag.z));
    EXPECT_GT(pressureHpa, 800.0f);
}

TEST(VirtualSensorAssemblyTest, SameSeedProducesDeterministicNoisyReadingsAcrossAssemblies) {
    VirtualSensorAssembly assemblyA;
    VirtualSensorAssembly assemblyB;

    assemblyA.setSeed(2468);
    assemblyB.setSeed(2468);

    assemblyA.imuSim().setAccelNoiseStdDev(0.01f);
    assemblyB.imuSim().setAccelNoiseStdDev(0.01f);
    assemblyA.imuSim().setGyroNoiseStdDev(0.005f);
    assemblyB.imuSim().setGyroNoiseStdDev(0.005f);

    Bmm350Simulator::ErrorConfig magErrors{};
    magErrors.noiseStdDev = 0.25f;
    assemblyA.magSim().setErrors(magErrors);
    assemblyB.magSim().setErrors(magErrors);

    assemblyA.baroSim().setPressureNoiseStdDev(0.1f);
    assemblyB.baroSim().setPressureNoiseStdDev(0.1f);

    ASSERT_TRUE(assemblyA.initAll());
    ASSERT_TRUE(assemblyB.initAll());

    assemblyA.gimbal().setRotationRate({0.1f, -0.2f, 0.3f});
    assemblyB.gimbal().setRotationRate({0.1f, -0.2f, 0.3f});
    assemblyA.gimbal().update(0.05f);
    assemblyB.gimbal().update(0.05f);
    assemblyA.gimbal().syncToSensors();
    assemblyB.gimbal().syncToSensors();

    AccelData accelA;
    AccelData accelB;
    GyroData gyroA;
    GyroData gyroB;
    MagData magA;
    MagData magB;
    float pressureA = 0.0f;
    float pressureB = 0.0f;

    ASSERT_TRUE(assemblyA.imuDriver().readAccel(accelA));
    ASSERT_TRUE(assemblyB.imuDriver().readAccel(accelB));
    ASSERT_TRUE(assemblyA.imuDriver().readGyro(gyroA));
    ASSERT_TRUE(assemblyB.imuDriver().readGyro(gyroB));
    ASSERT_TRUE(assemblyA.magDriver().readMag(magA));
    ASSERT_TRUE(assemblyB.magDriver().readMag(magB));
    ASSERT_TRUE(assemblyA.baroDriver().readPressure(pressureA));
    ASSERT_TRUE(assemblyB.baroDriver().readPressure(pressureB));

    EXPECT_FLOAT_EQ(accelA.x, accelB.x);
    EXPECT_FLOAT_EQ(accelA.y, accelB.y);
    EXPECT_FLOAT_EQ(accelA.z, accelB.z);
    EXPECT_FLOAT_EQ(gyroA.x, gyroB.x);
    EXPECT_FLOAT_EQ(gyroA.y, gyroB.y);
    EXPECT_FLOAT_EQ(gyroA.z, gyroB.z);
    EXPECT_FLOAT_EQ(magA.x, magB.x);
    EXPECT_FLOAT_EQ(magA.y, magB.y);
    EXPECT_FLOAT_EQ(magA.z, magB.z);
    EXPECT_FLOAT_EQ(pressureA, pressureB);
}
