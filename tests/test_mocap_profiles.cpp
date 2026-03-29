#include "MocapProfiles.hpp"
#include <gtest/gtest.h>

using helix::MocapPowerMode;
using helix::MocapProfile;
using helix::selectMocapProfile;

TEST(MocapProfilesTest, PerformanceProfileMatchesContract) {
    constexpr MocapProfile p = selectMocapProfile(MocapPowerMode::PERFORMANCE);
    static_assert(p.outputPeriodUs == 20000, "performance period");
    static_assert(p.imuOdrHz == 208, "performance imu odr");
    EXPECT_EQ(p.outputPeriodUs, 20000u);
    EXPECT_EQ(p.imuOdrHz, 208u);
    EXPECT_EQ(p.magOdrHz, 100u);
    EXPECT_EQ(p.baroOdrHz, 200u);
}

TEST(MocapProfilesTest, BatteryProfileMatchesContract) {
    constexpr MocapProfile p = selectMocapProfile(MocapPowerMode::BATTERY);
    static_assert(p.outputPeriodUs == 25000, "battery period");
    static_assert(p.baroOdrHz == 25, "battery baro odr");
    EXPECT_EQ(p.outputPeriodUs, 25000u);
    EXPECT_EQ(p.imuOdrHz, 104u);
    EXPECT_EQ(p.magOdrHz, 50u);
    EXPECT_EQ(p.baroOdrHz, 25u);
}
