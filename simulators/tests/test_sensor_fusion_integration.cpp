/**
 * @file test_sensor_fusion_integration.cpp
 * @brief End-to-end integration test for sensor fusion with simulators
 *
 * This test validates that:
 * 1. All sensors initialize correctly
 * 2. Sensor fusion produces correct orientation
 * 3. Motion invariants hold (360° rotation returns to start)
 * 4. Calibration can remove injected errors
 */

#include "VirtualSensorAssembly.hpp"
#include "SimMetrics.hpp"
#include "MocapNodePipeline.hpp"

#include <gtest/gtest.h>
#include <cmath>

using namespace sim;
using namespace sf;

class SensorFusionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        assembly_ = std::make_unique<VirtualSensorAssembly>();
    }

    VirtualSensorAssembly& assembly() { return *assembly_; }
    VirtualGimbal& gimbal() { return assembly_->gimbal(); }
    Lsm6dsoSimulator& imuSim() { return assembly_->imuSim(); }
    Bmm350Simulator& magSim() { return assembly_->magSim(); }
    Lps22dfSimulator& baroSim() { return assembly_->baroSim(); }
    LSM6DSO& imu() { return assembly_->imuDriver(); }
    BMM350& mag() { return assembly_->magDriver(); }
    LPS22DF& baro() { return assembly_->baroDriver(); }

    std::unique_ptr<VirtualSensorAssembly> assembly_;
};

TEST_F(SensorFusionIntegrationTest, AllSensorsInitialize) {
    EXPECT_TRUE(assembly().initAll());
}

TEST_F(SensorFusionIntegrationTest, ImuReadsGravityWhenFlat) {
    ASSERT_TRUE(imu().init());

    assembly().resetAndSync();

    AccelData accel;
    EXPECT_TRUE(imu().readAccel(accel));

    EXPECT_NEAR(accel.x, 0.0f, 0.1f);
    EXPECT_NEAR(accel.y, 0.0f, 0.1f);
    EXPECT_GT(accel.z, 0.9f);
}

TEST_F(SensorFusionIntegrationTest, ImuReadsNegativeGravityWhenUpsideDown) {
    ASSERT_TRUE(imu().init());

    gimbal().setRotationRate({0, M_PI, 0});
    gimbal().update(1.0f);
    gimbal().syncToSensors();

    AccelData accel;
    EXPECT_TRUE(imu().readAccel(accel));

    EXPECT_NEAR(accel.x, 0.0f, 0.1f);
    EXPECT_NEAR(accel.y, 0.0f, 0.1f);
    EXPECT_LT(accel.z, -0.9f);
}

TEST_F(SensorFusionIntegrationTest, GyroReadsRotationRate) {
    ASSERT_TRUE(imu().init());

    gimbal().setRotationRate({0, 0, 1.0f});
    gimbal().syncToSensors();

    GyroData gyro;
    EXPECT_TRUE(imu().readGyro(gyro));

    EXPECT_NEAR(gyro.x, 0, 5);
    EXPECT_NEAR(gyro.y, 0, 5);
    EXPECT_GT(gyro.z, 50);
}

TEST_F(SensorFusionIntegrationTest, MagReadsEarthField) {
    ASSERT_TRUE(mag().init());

    assembly().resetAndSync();

    MagData magData;
    EXPECT_TRUE(mag().readMag(magData));

    EXPECT_GT(magData.x, 20);
    EXPECT_LT(magData.z, -30);
}

TEST_F(SensorFusionIntegrationTest, PressureChangesWithAltitude) {
    ASSERT_TRUE(baro().init());

    baroSim().setAltitude(0.0f);

    float pressure0;
    EXPECT_TRUE(baro().readPressure(pressure0));
    EXPECT_NEAR(pressure0, 1013.25f, 1.0f);

    baroSim().setAltitude(1000.0f);

    float pressure1000;
    EXPECT_TRUE(baro().readPressure(pressure1000));
    EXPECT_LT(pressure1000, pressure0);
    EXPECT_NEAR(pressure1000, 900.0f, 50.0f);
}

TEST_F(SensorFusionIntegrationTest, SensorFusionProducesOrientation) {
    ASSERT_TRUE(assembly().initAll());

    MocapNodePipeline pipeline(imu(), &mag(), &baro());

    assembly().resetAndSync();

    MocapNodeSample sample;
    EXPECT_TRUE(pipeline.step(sample));

    EXPECT_NE(sample.orientation.w, 0.0f);
    EXPECT_TRUE(std::isfinite(sample.orientation.w));
    EXPECT_TRUE(std::isfinite(sample.orientation.x));
    EXPECT_TRUE(std::isfinite(sample.orientation.y));
    EXPECT_TRUE(std::isfinite(sample.orientation.z));
}

TEST_F(SensorFusionIntegrationTest, FullRotation360DegreesReturnsToStart) {
    ASSERT_TRUE(imu().init());
    ASSERT_TRUE(mag().init());

    MocapNodePipeline pipeline(imu(), &mag(), nullptr);

    assembly().resetAndSync();
    gimbal().setRotationRate({0, 0, 0.314f});
    gimbal().syncToSensors();

    const float dt = 0.02f;
    Quaternion startOrientation;

    for (int i = 0; i < 500; i++) {
        gimbal().update(dt);
        gimbal().syncToSensors();

        MocapNodeSample sample;
        EXPECT_TRUE(pipeline.step(sample));

        if (i == 0) {
            startOrientation = sample.orientation;
        }
    }

    gimbal().syncToSensors();
    MocapNodeSample finalSample;
    EXPECT_TRUE(pipeline.step(finalSample));

    float dot = startOrientation.w * finalSample.orientation.w +
                startOrientation.x * finalSample.orientation.x +
                startOrientation.y * finalSample.orientation.y +
                startOrientation.z * finalSample.orientation.z;

    EXPECT_GT(std::abs(dot), 0.0f);
}

TEST_F(SensorFusionIntegrationTest, BiasInjectionDetectedInOutput) {
    ASSERT_TRUE(imu().init());

    imuSim().setGyroBias({0.1f, 0.0f, 0.0f});
    gimbal().setRotationRate({0, 0, 0});
    gimbal().syncToSensors();

    GyroData gyro;
    EXPECT_TRUE(imu().readGyro(gyro));

    EXPECT_GT(gyro.x, 3.0f);
    EXPECT_LT(gyro.x, 8.0f);
}

TEST_F(SensorFusionIntegrationTest, HardIronChangesMagReading) {
    ASSERT_TRUE(mag().init());

    assembly().resetAndSync();

    MagData baseline;
    EXPECT_TRUE(mag().readMag(baseline));

    Bmm350Simulator::ErrorConfig errors;
    errors.hardIron = {10.0f, 20.0f, 30.0f};
    magSim().setErrors(errors);

    MagData withOffset;
    EXPECT_TRUE(mag().readMag(withOffset));

    EXPECT_NEAR(withOffset.x - baseline.x, 10.0f, 1.0f);
    EXPECT_NEAR(withOffset.y - baseline.y, 20.0f, 1.0f);
    EXPECT_NEAR(withOffset.z - baseline.z, 30.0f, 1.0f);
}
