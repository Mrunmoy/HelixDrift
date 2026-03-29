#include "VirtualMocapNodeHarness.hpp"

#include <gtest/gtest.h>

#include <cmath>

using namespace sim;

namespace {

float recoverRelativeAngleDeg(const sf::Quaternion& parent, const sf::Quaternion& child) {
    const sf::Quaternion relative = parent.conjugate().multiply(child);
    float clampedW = relative.w;
    if (clampedW > 1.0f) {
        clampedW = 1.0f;
    } else if (clampedW < -1.0f) {
        clampedW = -1.0f;
    }

    constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
    return std::abs(2.0f * std::acos(clampedW) * kRadToDeg);
}

} // namespace

TEST(PoseJointAngleTest, TwoNodeFlexionAnglesStayWithinTenDegrees) {
    constexpr float kFlexionAnglesDeg[] = {30.0f, 60.0f, 90.0f};

    for (float targetAngleDeg : kFlexionAnglesDeg) {
        VirtualMocapNodeHarness parent;
        VirtualMocapNodeHarness child;
        parent.setSeed(42);
        child.setSeed(43);

        ASSERT_TRUE(parent.initAll());
        ASSERT_TRUE(child.initAll());
        parent.resetAndSync();
        child.resetAndSync();

        child.assembly().gimbal().setOrientation(
            sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, targetAngleDeg));
        child.assembly().gimbal().syncToSensors();

        const NodeRunResult parentRun = parent.runWithWarmup(100, 200, 20000);
        const NodeRunResult childRun = child.runWithWarmup(100, 200, 20000);

        ASSERT_EQ(parentRun.samples.size(), 200u);
        ASSERT_EQ(childRun.samples.size(), 200u);

        const float recoveredAngleDeg = recoverRelativeAngleDeg(
            parentRun.samples.back().fusedOrientation,
            childRun.samples.back().fusedOrientation);
        const float errorDeg = std::abs(recoveredAngleDeg - targetAngleDeg);

        EXPECT_LT(errorDeg, 10.0f) << "target=" << targetAngleDeg;
    }
}
