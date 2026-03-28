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
