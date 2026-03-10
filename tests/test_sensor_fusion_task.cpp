#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "SensorFusionTask.hpp"
#include "MockSensors.hpp"

using namespace helix;
using namespace helix::mocks;
using namespace testing;

static constexpr float kDt = 1.0f / 200.0f;

// ── Helpers ───────────────────────────────────────────────────────────────

ACTION_P(SetAccel, val)
{
    arg0 = val;
}

ACTION_P(SetGyro, val)
{
    arg0 = val;
}

ACTION_P(SetMag, val)
{
    arg0 = val;
}

// ── 9-DOF path ────────────────────────────────────────────────────────────

TEST(SensorFusionTask, Tick9DOFReadsImuEveryTickMagOnDecim)
{
    MockAccelGyro imu;
    MockMag       mag;

    SensorFusionConfig cfg;
    cfg.magDecim = 4;
    SensorFusionTask task{imu, &mag, cfg};

    // Over 8 ticks: imu read 8×, mag read on ticks 0 and 4 → 2×
    EXPECT_CALL(imu, readAccel(_)).Times(8).WillRepeatedly(Return(true));
    EXPECT_CALL(imu, readGyro(_)).Times(8).WillRepeatedly(Return(true));
    EXPECT_CALL(mag, readMag(_)).Times(2).WillRepeatedly(Return(true));

    for (int i = 0; i < 8; ++i) { task.tick(kDt); }
}

TEST(SensorFusionTask, IsMagEnabledWhenMagProvided)
{
    MockAccelGyro imu;
    MockMag       mag;
    SensorFusionTask task{imu, &mag};
    EXPECT_TRUE(task.isMagEnabled());
}

TEST(SensorFusionTask, QuaternionUpdatesAfterTick)
{
    MockAccelGyro imu;
    MockMag       mag;
    SensorFusionTask task{imu, &mag};

    // Gravity in +X — not aligned with identity quaternion's reference down (+Z),
    // so Mahony has a non-zero error signal and will update the quaternion.
    sf::AccelData gravity{1.0f, 0.0f, 0.0f};
    sf::GyroData  zero{};
    sf::MagData   north{1.0f, 0.0f, 0.0f};

    EXPECT_CALL(imu, readAccel(_))
        .WillRepeatedly(DoAll(SetAccel(gravity), Return(true)));
    EXPECT_CALL(imu, readGyro(_))
        .WillRepeatedly(DoAll(SetGyro(zero), Return(true)));
    EXPECT_CALL(mag, readMag(_))
        .WillRepeatedly(DoAll(SetMag(north), Return(true)));

    // Identity quaternion before any tick.
    const sf::Quaternion before = task.getQuaternion();
    task.tick(kDt);
    const sf::Quaternion after = task.getQuaternion();

    // Mahony should have moved q — w < 1 after first update with real accel.
    EXPECT_NE(before.w, after.w);
}

// ── 6-DOF fallback ───────────────────────────────────────────────────────

TEST(SensorFusionTask, NullMagGives6DOF)
{
    MockAccelGyro imu;
    SensorFusionTask task{imu, nullptr};
    EXPECT_FALSE(task.isMagEnabled());
}

TEST(SensorFusionTask, Tick6DOFNeverReadsMag)
{
    MockAccelGyro imu;
    SensorFusionTask task{imu, nullptr};

    EXPECT_CALL(imu, readAccel(_)).Times(4).WillRepeatedly(Return(true));
    EXPECT_CALL(imu, readGyro(_)).Times(4).WillRepeatedly(Return(true));

    for (int i = 0; i < 4; ++i) { task.tick(kDt); }
}

// ── Sensor read failures (graceful) ──────────────────────────────────────

TEST(SensorFusionTask, AccelReadFailureDoesNotCrash)
{
    MockAccelGyro imu;
    MockMag       mag;
    SensorFusionTask task{imu, &mag};

    EXPECT_CALL(imu, readAccel(_)).WillOnce(Return(false));
    EXPECT_CALL(imu, readGyro(_)).WillOnce(Return(true));
    EXPECT_CALL(mag, readMag(_)).WillOnce(Return(true));

    EXPECT_NO_FATAL_FAILURE(task.tick(kDt));
}

TEST(SensorFusionTask, MagReadFailureDoesNotCrash)
{
    MockAccelGyro imu;
    MockMag       mag;
    SensorFusionTask task{imu, &mag};

    EXPECT_CALL(imu, readAccel(_)).WillOnce(Return(true));
    EXPECT_CALL(imu, readGyro(_)).WillOnce(Return(true));
    EXPECT_CALL(mag, readMag(_)).WillOnce(Return(false));

    EXPECT_NO_FATAL_FAILURE(task.tick(kDt));
}
