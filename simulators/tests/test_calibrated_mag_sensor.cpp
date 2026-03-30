#include "CalibratedMagSensor.hpp"

#include "CalibrationStore.hpp"
#include "INvStore.hpp"
#include "MagneticEnvironment.hpp"

#include <gtest/gtest.h>

namespace sim {
namespace {

class DummyNvStore : public sf::INvStore {
public:
    bool read(uint32_t, uint8_t*, size_t) override { return false; }
    bool write(uint32_t, const uint8_t*, size_t) override { return false; }
    size_t capacity() const override { return 0u; }
};

class FakeMagSensor : public sf::IMagSensor {
public:
    bool shouldSucceed = true;
    sf::MagData sample{0.0f, 0.0f, 0.0f};

    bool readMag(sf::MagData& out) override {
        if (!shouldSucceed) {
            return false;
        }
        out = sample;
        return true;
    }
};

} // namespace

TEST(CalibratedMagSensorTest, AppliesCalibrationOnSuccessfulRead) {
    DummyNvStore nv;
    sf::CalibrationStore store(nv);
    FakeMagSensor inner;
    inner.sample = {30.0f, -10.0f, 8.0f};

    sf::CalibrationData calibration{};
    calibration.offsetX = 10.0f;
    calibration.offsetY = -5.0f;
    calibration.offsetZ = 3.0f;

    CalibratedMagSensor sensor(inner, store, calibration);
    sf::MagData out{};

    ASSERT_TRUE(sensor.readMag(out));
    EXPECT_NEAR(out.x, 20.0f, 1.0e-4f);
    EXPECT_NEAR(out.y, -5.0f, 1.0e-4f);
    EXPECT_NEAR(out.z, 5.0f, 1.0e-4f);
}

TEST(CalibratedMagSensorTest, PropagatesReadFailureWithoutApplyingCalibration) {
    DummyNvStore nv;
    sf::CalibrationStore store(nv);
    FakeMagSensor inner;
    inner.shouldSucceed = false;

    CalibratedMagSensor sensor(inner, store);
    sf::MagData out{1.0f, 2.0f, 3.0f};

    EXPECT_FALSE(sensor.readMag(out));
    EXPECT_FLOAT_EQ(out.x, 1.0f);
    EXPECT_FLOAT_EQ(out.y, 2.0f);
    EXPECT_FLOAT_EQ(out.z, 3.0f);
}

TEST(CalibratedMagSensorTest, AppliesHardIronEstimateFromCalibrator) {
    DummyNvStore nv;
    sf::CalibrationStore store(nv);
    FakeMagSensor inner;
    inner.sample = {57.0f, -5.0f, 15.0f};

    HardIronCalibrator calibrator;
    calibrator.startCalibration();
    calibrator.addSample({57.0f, -5.0f, 3.0f});
    calibrator.addSample({-37.0f, -5.0f, 3.0f});
    calibrator.addSample({10.0f, 42.0f, 3.0f});
    calibrator.addSample({10.0f, -52.0f, 3.0f});
    calibrator.addSample({10.0f, -5.0f, 50.0f});
    calibrator.addSample({10.0f, -5.0f, -44.0f});

    CalibratedMagSensor sensor(inner, store);
    sensor.applyHardIronEstimate(calibrator);

    const sf::CalibrationData calibration = sensor.getCalibration();
    EXPECT_NEAR(calibration.offsetX, 10.0f, 1.0f);
    EXPECT_NEAR(calibration.offsetY, -5.0f, 1.0f);
    EXPECT_NEAR(calibration.offsetZ, 3.0f, 1.0f);

    sf::MagData out{};
    ASSERT_TRUE(sensor.readMag(out));
    EXPECT_NEAR(out.x, 47.0f, 1.0f);
    EXPECT_NEAR(out.y, 0.0f, 1.0f);
    EXPECT_NEAR(out.z, 12.0f, 1.0f);
}

TEST(CalibratedMagSensorTest, DisturbanceIndicatorReflectsAttachedEnvironment) {
    DummyNvStore nv;
    sf::CalibrationStore store(nv);
    FakeMagSensor inner;
    MagneticEnvironment env = MagneticEnvironment::cleanLab();

    CalibratedMagSensor sensor(inner, store);
    sensor.attachEnvironment(&env, {0.0f, 0.0f, 0.0f});
    EXPECT_LT(sensor.getDisturbanceIndicator(), 0.01f);

    env.addDipole({{0.1f, 0.0f, 0.0f}, {0.0f, 0.0f, 120.0f}, 1.0f});
    EXPECT_GT(sensor.getDisturbanceIndicator(), 0.2f);
}

} // namespace sim
