#include "VirtualGimbal.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using sim::VirtualGimbal;

TEST(MotionProfilesTest, CanonicalProfilesLoadSuccessfully) {
    const std::string root = std::string(HELIXDRIFT_SOURCE_DIR) + "/simulators/motion_profiles/";
    const std::vector<std::string> profiles = {
        "stationary/flat_60s.json",
        "stationary/tilted_30deg_60s.json",
        "single_axis/slow_yaw_360.json",
        "single_axis/slow_pitch_360.json",
        "single_axis/slow_roll_360.json",
        "single_axis/fast_yaw_360.json",
        "calibration/six_position_tumble.json",
        "calibration/gyro_static_10s.json",
        "calibration/gyro_rate_sweep.json",
        "calibration/figure_eight_tumble.json",
        "multi_axis/figure_eight.json",
        "multi_axis/walking_gait.json",
    };

    VirtualGimbal gimbal;
    for (const auto& profile : profiles) {
        EXPECT_TRUE(gimbal.loadMotionScript(root + profile)) << profile;
    }
}
