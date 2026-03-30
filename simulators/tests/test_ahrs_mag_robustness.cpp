#include "MagneticEnvironment.hpp"
#include "MocapNodePipeline.hpp"
#include "SimMetrics.hpp"
#include "VirtualSensorAssembly.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace sim {
namespace {

bool stepPipeline(VirtualSensorAssembly& assembly,
                  sf::MocapNodePipeline& pipeline,
                  uint64_t stepUs,
                  sf::MocapNodeSample& out) {
    assembly.gimbal().syncToSensors();
    assembly.advanceTimeUs(stepUs);
    return pipeline.step(out);
}

std::vector<float> runStationaryErrors(VirtualSensorAssembly& assembly,
                                       sf::MocapNodePipeline& pipeline,
                                       int steps,
                                       uint64_t stepUs) {
    std::vector<float> errors;
    errors.reserve(static_cast<std::size_t>(steps));
    sf::MocapNodeSample sample{};
    for (int i = 0; i < steps; ++i) {
        if (!stepPipeline(assembly, pipeline, stepUs, sample)) {
            return {};
        }
        errors.push_back(angularErrorDeg(assembly.gimbal().getOrientation(), sample.orientation));
    }
    return errors;
}

} // namespace

TEST(AHRSMagRobustnessTest, HeadingRecoversAfterTemporaryMagneticDisturbance) {
    VirtualSensorAssembly assembly;
    assembly.setSeed(42);

    MagneticEnvironment environment = MagneticEnvironment::cleanLab();
    assembly.magSim().attachEnvironment(&environment, {0.0f, 0.0f, 0.0f});

    ASSERT_TRUE(assembly.initAll());

    sf::MocapNodePipeline::Config config{};
    config.mahonyKp = 1.0f;
    config.mahonyKi = 0.02f;
    sf::MocapNodePipeline pipeline(assembly.imuDriver(), &assembly.magDriver(), &assembly.baroDriver(), config);

    assembly.resetAndSync();
    assembly.gimbal().setOrientation(sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 45.0f));
    assembly.gimbal().syncToSensors();

    const std::vector<float> cleanErrors = runStationaryErrors(assembly, pipeline, 150, 20000);
    ASSERT_EQ(cleanErrors.size(), 150u);
    const ErrorSeriesStats cleanStats = summarizeErrorSeriesDeg(cleanErrors);

    environment.addDipole({{0.08f, 0.0f, 0.0f}, {0.0f, 0.0f, 140.0f}, 0.5f});
    const MagneticEnvironment::FieldQuality disturbedQuality = environment.getQualityAt({0.0f, 0.0f, 0.0f});
    ASSERT_TRUE(disturbedQuality.isDisturbed);

    const std::vector<float> disturbedErrors = runStationaryErrors(assembly, pipeline, 100, 20000);
    ASSERT_EQ(disturbedErrors.size(), 100u);
    const ErrorSeriesStats disturbedStats = summarizeErrorSeriesDeg(disturbedErrors);

    environment.clearDipoles();
    const std::vector<float> recoveryErrors = runStationaryErrors(assembly, pipeline, 200, 20000);
    ASSERT_EQ(recoveryErrors.size(), 200u);
    const ErrorSeriesStats recoveryStats = summarizeErrorSeriesDeg(recoveryErrors);
    const float recoveryFinalDeg = recoveryErrors.back();

    EXPECT_LT(cleanStats.rmsDeg, 8.0f);
    EXPECT_GT(disturbedStats.maxDeg, cleanStats.maxDeg + 2.0f);
    EXPECT_LT(recoveryFinalDeg, disturbedStats.maxDeg);
    EXPECT_LT(recoveryStats.rmsDeg, 10.0f);
}

} // namespace sim
