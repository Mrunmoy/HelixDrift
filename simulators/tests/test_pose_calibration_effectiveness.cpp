#include "CalibratedMagSensor.hpp"
#include "CalibrationFitter.hpp"
#include "CalibrationStore.hpp"
#include "INvStore.hpp"
#include "MocapNodePipeline.hpp"
#include "SimMetrics.hpp"
#include "VirtualSensorAssembly.hpp"

#include <array>
#include <vector>

#include <gtest/gtest.h>

using namespace sim;

namespace {

class DummyNvStore : public sf::INvStore {
public:
    bool read(uint32_t, uint8_t*, size_t) override { return false; }
    bool write(uint32_t, const uint8_t*, size_t) override { return false; }
    size_t capacity() const override { return 0u; }
};

bool stepPipeline(VirtualSensorAssembly& assembly,
                  sf::MocapNodePipeline& pipeline,
                  uint64_t stepUs,
                  sf::MocapNodeSample& out) {
    assembly.gimbal().syncToSensors();
    assembly.delay().timestampUs += stepUs;
    return pipeline.step(out);
}

std::vector<float> runStaticPoseErrors(float yawDeg,
                                       const sim::Bmm350Simulator::ErrorConfig& errors,
                                       const sf::CalibrationData* calibration = nullptr) {
    VirtualSensorAssembly assembly;
    assembly.setSeed(42);
    assembly.magSim().setErrors(errors);
    if (!assembly.initAll()) {
        ADD_FAILURE() << "VirtualSensorAssembly initAll() failed";
        return {};
    }

    DummyNvStore nv;
    sf::CalibrationStore store(nv);
    sim::CalibratedMagSensor calibratedMag(assembly.magDriver(),
                                           store,
                                           calibration ? *calibration : sf::CalibrationData{});
    sf::IMagSensor* magSensor = calibration ? static_cast<sf::IMagSensor*>(&calibratedMag)
                                            : static_cast<sf::IMagSensor*>(&assembly.magDriver());

    sf::MocapNodePipeline pipeline(assembly.imuDriver(), magSensor, &assembly.baroDriver());

    assembly.resetAndSync();
    assembly.gimbal().setOrientation(sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, yawDeg));
    assembly.gimbal().syncToSensors();

    sf::MocapNodeSample sample{};
    for (int i = 0; i < 100; ++i) {
        if (!stepPipeline(assembly, pipeline, 20000, sample)) {
            ADD_FAILURE() << "Warmup pipeline step failed";
            return {};
        }
    }

    std::vector<float> errorsDeg;
    errorsDeg.reserve(200);
    for (int i = 0; i < 200; ++i) {
        if (!stepPipeline(assembly, pipeline, 20000, sample)) {
            ADD_FAILURE() << "Measurement pipeline step failed";
            return {};
        }
        errorsDeg.push_back(angularErrorDeg(assembly.gimbal().getOrientation(), sample.orientation));
    }
    return errorsDeg;
}

sf::CalibrationData fitHardIronCalibration(const sim::Bmm350Simulator::ErrorConfig& errors) {
    VirtualSensorAssembly assembly;
    assembly.setSeed(42);
    assembly.magSim().setErrors(errors);
    if (!assembly.initAll()) {
        ADD_FAILURE() << "VirtualSensorAssembly initAll() failed";
        return {};
    }

    const std::array<sf::Quaternion, 6> poses = {
        sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 0.0f),
        sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 180.0f),
        sf::Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, 90.0f),
        sf::Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, -90.0f),
        sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 90.0f),
        sf::Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, -90.0f),
    };

    std::array<sf::MagData, poses.size()> samples{};
    for (std::size_t i = 0; i < poses.size(); ++i) {
        assembly.gimbal().setOrientation(poses[i]);
        assembly.gimbal().syncToSensors();
        if (!assembly.magDriver().readMag(samples[i])) {
            ADD_FAILURE() << "Failed to read calibration magnetometer sample at index " << i;
            return {};
        }
    }

    sf::CalibrationData calibration{};
    EXPECT_TRUE(sf::CalibrationFitter::fitMagHardSoftIron(samples.data(), samples.size(), calibration));
    return calibration;
}

} // namespace

TEST(PoseCalibrationEffectivenessTest, HardIronFitImprovesStaticYawAccuracyByMoreThan2x) {
    sim::Bmm350Simulator::ErrorConfig errors{};
    errors.hardIron = {15.0f, -10.0f, 5.0f};

    const sf::CalibrationData calibration = fitHardIronCalibration(errors);
    const std::vector<float> uncalibratedErrors = runStaticPoseErrors(90.0f, errors);
    const std::vector<float> calibratedErrors = runStaticPoseErrors(90.0f, errors, &calibration);

    ASSERT_EQ(uncalibratedErrors.size(), 200u);
    ASSERT_EQ(calibratedErrors.size(), 200u);

    const ErrorSeriesStats uncalibrated = summarizeErrorSeriesDeg(uncalibratedErrors);
    const ErrorSeriesStats calibrated = summarizeErrorSeriesDeg(calibratedErrors);

    EXPECT_GT(uncalibrated.rmsDeg, 8.0f);
    EXPECT_LT(calibrated.rmsDeg, 5.0f);
    EXPECT_LT(calibrated.rmsDeg, uncalibrated.rmsDeg * 0.5f);
}
