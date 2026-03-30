#pragma once

#include "CalibrationStore.hpp"
#include "HardIronCalibrator.hpp"
#include "SensorInterface.hpp"

namespace sim {

class MagneticEnvironment;

class CalibratedMagSensor : public sf::IMagSensor {
public:
    CalibratedMagSensor(sf::IMagSensor& inner,
                        sf::CalibrationStore& store,
                        const sf::CalibrationData& calibration = {});

    bool readMag(sf::MagData& out) override;

    void setCalibration(const sf::CalibrationData& calibration);
    const sf::CalibrationData& getCalibration() const { return calibration_; }
    void clearCalibration();
    void applyHardIronEstimate(const HardIronCalibrator& calibrator);
    void attachEnvironment(const MagneticEnvironment* environment, const sf::Vec3& position);
    float getDisturbanceIndicator() const;

private:
    sf::IMagSensor& inner_;
    sf::CalibrationStore& store_;
    sf::CalibrationData calibration_{};
    const MagneticEnvironment* environment_ = nullptr;
    sf::Vec3 position_{0.0f, 0.0f, 0.0f};
};

} // namespace sim
